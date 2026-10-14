#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Steven Azevedo
# Installs the freshly built bootR.efi (from `make`) to the EFI System Partition. See packaging/boot-r-install.
here="$(cd "$(dirname "$0")" && pwd)"
[ -f "$here/bootR.efi" ] || { echo "Run 'make' first."; exit 1; }
BOOTR_EFI="$here/bootR.efi" exec "$here/packaging/boot-r-install" "$@"
