# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Steven Azevedo
"""Builds boot-r_<version>_amd64.deb without dpkg (runs on macOS and Linux).
Usage: mkdeb.py <bootR.efi> <out.deb>
A .deb is an `ar` archive of: debian-binary, control.tar.gz, data.tar.gz."""
import gzip, hashlib, io, os, sys, tarfile, time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PK = f"{ROOT}/packaging"
VERSION = "1.0.0"
MAINTAINER = "Steven Azevedo <steven@azevedomedia.com>"
NOW = int(os.environ.get("SOURCE_DATE_EPOCH", time.time()))
efi_path, out = sys.argv[1], sys.argv[2]

def rd(p): return open(p, "rb").read()
def gz(b): return gzip.compress(b, 9, mtime=0)

CHANGELOG = f"""boot-r ({VERSION}) unstable; urgency=low

  * Initial release.

 -- {MAINTAINER}  Tue, 13 Oct 2026 23:40:00 +0000
""".encode()

files = {  # path in package -> (mode, content)
    "usr/sbin/boot-r-install": (0o755, rd(f"{PK}/boot-r-install")),
    "usr/share/boot-r/bootR.efi": (0o644, rd(efi_path)),
    "usr/share/applications/io.bootr.BootR.desktop": (0o644, rd(f"{PK}/io.bootr.BootR.desktop")),
    "usr/share/metainfo/io.bootr.BootR.metainfo.xml": (0o644, rd(f"{PK}/io.bootr.BootR.metainfo.xml")),
    "usr/share/man/man8/boot-r-install.8.gz": (0o644, gz(rd(f"{PK}/boot-r-install.8"))),
    "usr/share/doc/boot-r/copyright": (0o644, rd(f"{PK}/copyright")),
    "usr/share/doc/boot-r/changelog.gz": (0o644, gz(CHANGELOG)),
}
for n in (16, 32, 48, 64, 128, 256, 512):
    files[f"usr/share/icons/hicolor/{n}x{n}/apps/boot-r.png"] = (0o644, rd(f"{ROOT}/assets/icons/boot-r-{n}.png"))

def tar_bytes(entries):
    """entries: list of (name, mode, content or None for a directory)."""
    buf = io.BytesIO()
    with tarfile.open(fileobj=buf, mode="w", format=tarfile.GNU_FORMAT) as t:
        for name, mode, content in entries:
            ti = tarfile.TarInfo(name)
            ti.uid = ti.gid = 0; ti.uname = ti.gname = "root"; ti.mtime = NOW; ti.mode = mode
            if content is None:
                ti.type = tarfile.DIRTYPE; t.addfile(ti)
            else:
                ti.size = len(content); t.addfile(ti, io.BytesIO(content))
    return gz(buf.getvalue())

dirs = sorted({"/".join(p.split("/")[:i]) for p in files for i in range(1, len(p.split("/")))})
data_entries = [("./", 0o755, None)] + [(f"./{d}/", 0o755, None) for d in dirs] + \
               [(f"./{p}", m, c) for p, (m, c) in sorted(files.items())]
data_tar = tar_bytes(data_entries)

installed_kib = -(-sum(len(c) for _, c in files.values()) // 1024)
control = f"""Package: boot-r
Version: {VERSION}
Architecture: amd64
Maintainer: {MAINTAINER}
Installed-Size: {installed_kib}
Depends: efibootmgr, util-linux
Section: admin
Homepage: https://github.com/azevedomedia0/boot.R
Priority: optional
Description: graphical UEFI boot manager
 boot.R is a small boot manager for UEFI computers. It scans your drives for
 installed operating systems, shows them as large tiles that you pick with the
 arrow keys, and starts the one you choose. It supports a default system, an
 auto-boot countdown, and renaming, hiding and reordering entries.
 .
 Installing this package copies the files into place only. Run
 "sudo boot-r-install" to add boot.R to the EFI System Partition as the first
 boot entry. It is unsigned, so Secure Boot must be off.
""".encode()
md5s = "".join(f"{hashlib.md5(c).hexdigest()}  {p}\n" for p, (_, c) in sorted(files.items())).encode()
control_tar = tar_bytes([("./", 0o755, None), ("./control", 0o644, control), ("./md5sums", 0o644, md5s),
                         ("./postinst", 0o755, rd(f"{PK}/postinst")), ("./prerm", 0o755, rd(f"{PK}/prerm"))])

def ar_member(name, b):
    hdr = f"{name:<16}{NOW:<12}{0:<6}{0:<6}{'100644':<8}{len(b):<10}`\n".encode()
    return hdr + b + (b"\n" if len(b) % 2 else b"")
with open(out, "wb") as f:
    f.write(b"!<arch>\n" + ar_member("debian-binary", b"2.0\n") + ar_member("control.tar.gz", control_tar)
            + ar_member("data.tar.gz", data_tar))
print(f"{out}: {os.path.getsize(out)} bytes, {len(files)} files, Installed-Size {installed_kib} KiB")
