#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EFI_SRC="${ROOT_DIR}/build/kernel.so"
EFI_DST_DIR="${ROOT_DIR}/esp/EFI/BOOT"
EFI_DST="${EFI_DST_DIR}/BOOTX64.EFI"

mkdir -p "${EFI_DST_DIR}"

x86_64-elf-objcopy \
  -I elf64-x86-64 -O pei-x86-64 --subsystem=10 \
  -j .text -j .sdata -j .data -j .rodata -j .dynamic -j .dynsym -j .rel -j .rela -j .reloc \
  "${EFI_SRC}" "${EFI_DST}"

echo "Wrote ${EFI_DST}"
