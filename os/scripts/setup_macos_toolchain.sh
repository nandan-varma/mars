#!/usr/bin/env bash
set -euo pipefail

# Minimal setup for MarsOS on macOS.
# Preferred path uses mingw-w64 to produce BOOTX64.EFI directly.

if ! command -v brew >/dev/null 2>&1; then
  echo "Homebrew is required: https://brew.sh"
  exit 1
fi

echo "Installing baseline dependencies..."
brew install qemu mingw-w64 x86_64-elf-gcc x86_64-elf-binutils mtools gptfdisk || true

echo "Ensuring x86_64 MinGW compiler exists..."
if ! command -v x86_64-w64-mingw32-gcc >/dev/null 2>&1; then
  echo "Could not find x86_64-w64-mingw32-gcc after install."
  exit 1
fi

echo "Toolchain status:"
for tool in x86_64-w64-mingw32-gcc x86_64-elf-gcc x86_64-elf-ld x86_64-elf-objcopy qemu-system-x86_64; do
  if command -v "$tool" >/dev/null 2>&1; then
    printf "  [ok] %s -> %s\n" "$tool" "$(command -v "$tool")"
  else
    printf "  [missing] %s\n" "$tool"
  fi
done

echo

echo "Next steps:"
echo "  cd os"
echo "  make all"
echo "  make run"
