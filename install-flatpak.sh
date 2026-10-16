#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Steven Azevedo
# Installs the compiler (Flatpak SDK + LLVM), builds bootR.efi inside the Flatpak, installs the Flatpak, runs it to
# add boot.R to the EFI partition, makes boot.R the first boot entry, then shows an alert. Run from a terminal.
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
LABEL="boot.R"
APP_ID="io.bootr.BootR"
BRANCH="25.08"

# Alert window: zenity, then kdialog, then a desktop notification, then the terminal.
alert() {
  local kind="$1" msg="$2"
  if [ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ] && command -v zenity >/dev/null; then
    zenity "--$kind" --title="boot.R" --width=360 --text="$msg" 2>/dev/null || true
  elif [ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ] && command -v kdialog >/dev/null; then
    [ "$kind" = error ] && kdialog --title "boot.R" --error "$msg" || kdialog --title "boot.R" --msgbox "$msg"
  elif command -v notify-send >/dev/null; then
    notify-send -a "boot.R" "boot.R" "$msg" || true
  fi
  echo "$msg"
}

step() { printf '\n==> %s\n' "$*"; }
# Root commands: sudo in a terminal, otherwise the desktop's graphical password prompt (polkit).
as_root() { if [ -t 0 ]; then sudo "$@"; else pkexec "$@"; fi; }
trap 'alert error "boot.R install failed at line $LINENO. See the terminal for details."' ERR

[ "$(id -u)" != 0 ] || { echo "Run this as your normal user; it asks for your password when needed." >&2; exit 1; }
[ -d /sys/firmware/efi ] || { alert error "This system was not booted in UEFI mode."; exit 1; }

step "Installing host tools (flatpak, flatpak-builder, efibootmgr, polkit, zenity)"
need=()
for pair in flatpak:flatpak flatpak-builder:flatpak-builder efibootmgr:efibootmgr pkexec:pkexec zenity:zenity; do
  command -v "${pair%%:*}" >/dev/null || need+=("${pair#*:}")
done
if [ ${#need[@]} -gt 0 ]; then
  as_root sh -c 'apt-get update && apt-get install -y "$@"' sh "${need[@]}"
fi

step "Installing the compiler (Flatpak SDK $BRANCH + LLVM 20 extension)"
flatpak remote-add --user --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
flatpak install --user -y --noninteractive flathub \
  "org.freedesktop.Platform//$BRANCH" "org.freedesktop.Sdk//$BRANCH" "org.freedesktop.Sdk.Extension.llvm20//$BRANCH"

step "Building bootR.efi and installing the $APP_ID Flatpak"
make -C "$here" flatpak

step "Adding boot.R to the EFI partition (polkit will ask for your password)"
flatpak run "$APP_ID"

step "Making $LABEL the first boot entry"
num="$(efibootmgr | sed -n "s/^Boot\([0-9A-Fa-f]\{4\}\)\*\{0,1\} $LABEL\(\t.*\)\{0,1\}\$/\1/p" | head -n1)"
[ -n "$num" ] || { alert error "The $LABEL boot entry was not created."; exit 1; }
order="$(efibootmgr | sed -n 's/^BootOrder: //p')"
rest="$(tr ',' '\n' <<<"$order" | grep -vix "$num" | paste -sd, -)"
if [ "${order%%,*}" != "$num" ]; then
  as_root efibootmgr --bootorder "$num${rest:+,$rest}" >/dev/null
fi
efibootmgr | grep -E '^BootOrder:'

trap - ERR
alert info "boot.R was installed successfully.

It is now the first boot entry (Boot$num). Restart to use it.
Remove it with: flatpak run io.bootr.BootR --uninstall"
