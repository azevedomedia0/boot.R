# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Steven Azevedo
"""Builds a USB-ready raw disk image: protective MBR + GPT + one EFI System Partition (FAT32)
holding \\EFI\\BOOT\\BOOTX64.EFI. No mounting, no root, byte-for-byte reproducible.
Usage: mkimage.py <bootR.efi> <out.img> [size-MiB, default 64]"""
import struct, sys, uuid, zlib

efi = open(sys.argv[1], "rb").read()
out = sys.argv[2]
size_mib = int(sys.argv[3]) if len(sys.argv) > 3 else 64
SEC = 512
total = size_mib * 1024 * 1024 // SEC
part_start = 2048                      # 1 MiB alignment
part_end = total - 34                  # leave room for the backup GPT (32 entry sectors + header)
part_secs = part_end - part_start + 1

# ---- FAT32 ----
RES, NFATS, SPC = 32, 2, 1
fat_secs = 1                           # smallest FAT that can index every cluster the rest of the partition holds
while True:
    clusters = (part_secs - RES - NFATS * fat_secs) // SPC
    if fat_secs * (SEC // 4) >= clusters + 2:
        break
    fat_secs += 1
assert clusters >= 65525, "too small for FAT32"
data_start = RES + NFATS * fat_secs
fat = [0] * (clusters + 2)
fat[0], fat[1] = 0x0FFFFFF8, 0x0FFFFFFF
data = {}                              # cluster -> 512-byte sector contents
nxt = [2]

def alloc(n):
    first = nxt[0]
    for i in range(n):
        fat[first + i] = first + i + 1 if i < n - 1 else 0x0FFFFFFF
    nxt[0] += n
    return first

DATE, TIME = ((2026 - 1980) << 9) | (1 << 5) | 1, 0
def ent(name, attr, cl, sz):
    return struct.pack("<11sBBBHHHHHHHI", name, attr, 0, 0, TIME, DATE, DATE, cl >> 16, TIME, DATE, cl & 0xFFFF, sz)

def put_dir(cl, entries):
    raw = b"".join(entries).ljust(SEC, b"\0")
    assert len(raw) == SEC
    data[cl] = raw

root, d_efi, d_boot = alloc(1), alloc(1), alloc(1)
nclu = (len(efi) + SEC * SPC - 1) // (SEC * SPC)
f_cl = alloc(nclu)
for i in range(nclu):
    data[f_cl + i] = efi[i * SEC:(i + 1) * SEC].ljust(SEC, b"\0")
put_dir(root, [ent(b"BOOTR      ", 0x08, 0, 0), ent(b"EFI        ", 0x10, d_efi, 0)])
put_dir(d_efi, [ent(b".          ", 0x10, d_efi, 0), ent(b"..         ", 0x10, 0, 0), ent(b"BOOT       ", 0x10, d_boot, 0)])
put_dir(d_boot, [ent(b".          ", 0x10, d_boot, 0), ent(b"..         ", 0x10, d_efi, 0),
                 ent(b"BOOTX64 EFI", 0x20, f_cl, len(efi))])

bpb = struct.pack("<3s8sHBHBHHBHHHIIIHHIHH12sBBBI11s8s",
                  b"\xEB\x58\x90", b"BOOTR   ", SEC, SPC, RES, NFATS, 0, 0, 0xF8, 0, 32, 64, part_start,
                  part_secs, fat_secs, 0, 0, 2, 1, 6, b"\0" * 12, 0x80, 0, 0x29, 0x426F6F74,
                  b"BOOTR      ", b"FAT32   ")
boot = bpb.ljust(510, b"\0") + b"\x55\xAA"
free = clusters - (nxt[0] - 2)
fsinfo = (struct.pack("<I", 0x41615252) + b"\0" * 480 + struct.pack("<III", 0x61417272, free, nxt[0])
          + b"\0" * 12 + struct.pack("<I", 0xAA550000))

img = bytearray(total * SEC)
def w(lba, b): img[lba * SEC:lba * SEC + len(b)] = b
p = part_start
for base in (0, 6):
    w(p + base, boot); w(p + base + 1, fsinfo)
fatb = struct.pack("<%dI" % len(fat), *fat).ljust(fat_secs * SEC, b"\0")
w(p + RES, fatb); w(p + RES + fat_secs, fatb)
for cl, raw in data.items():
    w(p + data_start + (cl - 2) * SPC, raw)

# ---- GPT ----
ESP = uuid.UUID("C12A7328-F81F-11D2-BA4B-00A0C93EC93B")
disk_guid = uuid.UUID("b007b007-b007-4007-8007-b007b007b007")
part_guid = uuid.UUID("b007b007-b007-4007-8007-b007b007b008")
name = "boot.R".encode("utf-16-le").ljust(72, b"\0")
entry = (ESP.bytes_le + part_guid.bytes_le + struct.pack("<QQQ", part_start, part_end, 0) + name).ljust(128, b"\0")
entries = entry.ljust(128 * 128, b"\0")
ecrc = zlib.crc32(entries)
def header(cur, other, first_ent):
    h = struct.pack("<8sIIIIQQQQ16sQIII", b"EFI PART", 0x10000, 92, 0, 0, cur, other, 34, total - 34,
                    disk_guid.bytes_le, first_ent, 128, 128, ecrc)
    h = h[:16] + struct.pack("<I", zlib.crc32(h[:16] + b"\0\0\0\0" + h[20:])) + h[20:]
    return h.ljust(SEC, b"\0")
w(1, header(1, total - 1, 2)); w(2, entries)
w(total - 33, entries); w(total - 1, header(total - 1, 1, total - 33))
mbr = bytearray(SEC)
mbr[446:462] = struct.pack("<BBBBBBBBII", 0, 0, 2, 0, 0xEE, 0xFF, 0xFF, 0xFF, 1, min(total - 1, 0xFFFFFFFF))
mbr[510:512] = b"\x55\xAA"
w(0, bytes(mbr))
open(out, "wb").write(img)
print(f"{out}: {size_mib} MiB, ESP at LBA {part_start}, {clusters} clusters, bootR.efi = {len(efi)} bytes")
