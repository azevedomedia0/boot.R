#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Steven Azevedo
# Builds bootR.efi with clang + lld-link (the Flatpak SDK has no mingw). Run from the repository root.
set -eu
CFLAGS="--target=x86_64-pc-windows-msvc -std=gnu11 -O2 -Wall -ffreestanding -fshort-wchar -fno-stack-protector \
  -mno-red-zone -mno-stack-arg-probe -fno-builtin -fno-asynchronous-unwind-tables -fno-unwind-tables"
objs=""
for f in main gfx store assets icon rt; do
  clang $CFLAGS -c "src/$f.c" -o "src/$f.o"
  objs="$objs src/$f.o"
done
lld-link /nodefaultlib /subsystem:efi_application /entry:efi_main /base:0x100000 /filealign:512 /opt:ref \
  /out:bootR.efi $objs
