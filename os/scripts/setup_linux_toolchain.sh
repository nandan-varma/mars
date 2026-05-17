#!/usr/bin/env bash
set -euo pipefail

# Minimal setup for MarsOS on Debian/Ubuntu.
# Preferred path uses mingw-w64 to produce BOOTX64.EFI directly,
# matching the macOS setup script.

if ! command -v apt-get >/dev/null 2>&1; then
  echo "This script targets Debian/Ubuntu. For other distros, install:"
  echo "  gcc-mingw-w64-x86-64, build-essential, qemu-system-x86, ovmf"
  exit 1
fi

SUDO=""
if [ "$(id -u)" -ne 0 ]; then
  SUDO="sudo"
fi

echo "Installing baseline dependencies..."
$SUDO apt-get update -y
$SUDO apt-get install -y --no-install-recommends \
  build-essential \
  gcc-mingw-w64-x86-64 \
  qemu-system-x86 \
  ovmf \
  ca-certificates \
  make \
  mtools \
  gdisk

echo "Ensuring x86_64 MinGW compiler exists..."
if ! command -v x86_64-w64-mingw32-gcc >/dev/null 2>&1; then
  echo "Could not find x86_64-w64-mingw32-gcc after install."
  exit 1
fi

echo "Toolchain status:"
for tool in x86_64-w64-mingw32-gcc cc make qemu-system-x86_64; do
  if command -v "$tool" >/dev/null 2>&1; then
    printf "  [ok] %s -> %s\n" "$tool" "$(command -v "$tool")"
  else
    printf "  [missing] %s\n" "$tool"
  fi
done

# OVMF firmware (used by `make run` and the QEMU smoke test in M2).
for path in \
  /usr/share/OVMF/OVMF_CODE.fd \
  /usr/share/OVMF/OVMF_CODE_4M.fd \
  /usr/share/ovmf/OVMF.fd; do
  if [ -f "$path" ]; then
    printf "  [ok] OVMF code -> %s\n" "$path"
    break
  fi
done

echo
echo "Next steps:"
echo "  cd os"
echo "  make all"
echo "  make run     # requires OVMF_CODE if not auto-detected"
