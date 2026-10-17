# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Steven Azevedo
# Standalone: needs only a mingw-w64 x86_64 cross compiler (no gnu-efi, no EDK2).
#   macOS:  brew install mingw-w64        Debian/Ubuntu: apt install gcc-mingw-w64-x86-64
ifeq ($(origin CC),default)
CC      := x86_64-w64-mingw32-gcc
endif
CFLAGS  := -std=gnu11 -O2 -Wall -ffreestanding -fshort-wchar -fno-stack-protector -fno-stack-check \
           -mno-red-zone -fno-builtin -fno-tree-loop-distribute-patterns -fno-asynchronous-unwind-tables -fno-ident
LDFLAGS := -nostdlib -Wl,--subsystem,10 -Wl,-e,efi_main -Wl,--image-base,0x100000 -Wl,--file-alignment,512 \
           -Wl,--section-alignment,4096 -Wl,--gc-sections -Wl,--no-insert-timestamp -s
OBJS    := src/main.o src/gfx.o src/store.o src/mouse.o src/hid.o src/i2chid.o src/assets.o src/logos2.o src/icon.o src/rt.o assets/bootR.res.o
WINDRES ?= $(subst gcc,windres,$(CC))
# For `make demo` (needs qemu-system-x86_64 and an x86_64 UEFI firmware image)
QEMU    ?= qemu-system-x86_64
OVMF    ?= $(firstword $(wildcard /opt/homebrew/share/qemu/edk2-x86_64-code.fd /usr/share/qemu/edk2-x86_64-code.fd /usr/share/ovmf/OVMF.fd))

all: bootR.efi

src/%.o: src/%.c src/bm.h src/uefi.h
	$(CC) $(CFLAGS) -c $< -o $@

# Windows resource: the boot.R icon and version info, embedded in the .efi
assets/bootR.res.o: assets/bootR.rc assets/bootR.ico
	$(WINDRES) -O coff -i $< -o $@

bootR.efi: $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

# Copy dist/ to a FAT32 USB stick (or the ESP) and boot it: it is the removable-media fallback loader.
dist: bootR.efi
	rm -rf dist && mkdir -p dist/EFI/BOOT dist/EFI/bootR
	cp bootR.efi dist/EFI/BOOT/BOOTX64.EFI
	cp bootR.efi dist/EFI/bootR/bootR.efi

# Raw disk image (GPT + FAT32 EFI System Partition) for `dd` / Etcher / Raspberry Pi Imager.
bootR.img: bootR.efi tools/mkimage.py
	python3 tools/mkimage.py bootR.efi $@ 64

image: bootR.img

# Debian package, for apt / software centres. Built without dpkg, so it works on macOS too.
DEB := boot-r_1.0.0_amd64.deb
$(DEB): bootR.efi tools/mkdeb.py $(wildcard packaging/*) $(wildcard assets/icons/*)
	python3 tools/mkdeb.py bootR.efi $@

deb: $(DEB)

# Flatpak (self-hosted; needs flatpak-builder on Linux). Builds and installs it for the current user.
flatpak:
	flatpak-builder --user --install --install-deps-from=flathub --force-clean build-flatpak packaging/flatpak/io.bootr.BootR.yml

# Fake ESP in ./esp whose "systems" are copies of the menu itself.
esp: bootR.efi
	rm -rf esp && mkdir -p esp/EFI/BOOT esp/EFI/ubuntu esp/EFI/Microsoft/Boot esp/EFI/debian esp/EFI/fedora esp/EFI/freebsd esp/EFI/Pop_OS-demo
	cp bootR.efi esp/EFI/BOOT/BOOTX64.EFI
	cp bootR.efi esp/EFI/ubuntu/shimx64.efi
	cp bootR.efi esp/EFI/Microsoft/Boot/bootmgfw.efi
	cp bootR.efi esp/EFI/debian/grubx64.efi
	cp bootR.efi esp/EFI/fedora/shimx64.efi
	cp bootR.efi esp/EFI/freebsd/loader.efi
	cp bootR.efi esp/EFI/Pop_OS-demo/systemd-bootx64.efi

demo: esp
	$(QEMU) -m 512 -bios $(OVMF) -drive file=fat:rw:esp,format=raw -device VGA -display cocoa 2>/dev/null || \
	$(QEMU) -m 512 -bios $(OVMF) -drive file=fat:rw:esp,format=raw -device VGA

clean:
	rm -f src/*.o assets/*.o bootR.efi
	rm -rf esp dist bootR.img boot-r_*.deb build-flatpak .flatpak-builder

.PHONY: all dist image deb flatpak esp demo clean
