#!/usr/bin/env bash
set -euo pipefail

# Build a bootable USB image for real-hardware MarsOS.
#
# Produces a hybrid GPT+ESP image at $OUT (default os/build/mars-usb.img)
# that can be written directly to a USB stick with:
#   sudo dd if=os/build/mars-usb.img of=/dev/sdX bs=4M status=progress
#
# The image contains a single GPT partition formatted as FAT32, containing
# /EFI/BOOT/BOOTX64.EFI. UEFI firmware sees the ESP and boots it.
#
# Tunables:
#   OUT          output image path (default os/build/mars-usb.img)
#   IMAGE_MB     total image size in MiB (default 128 — small but
#                comfortable for the ESP plus a future kernel manifest)
#   ESP_LABEL    FAT32 volume label (default MARSOS)
#
# Required tools:
#   - sgdisk (gptfdisk on macOS, gdisk on Debian)
#   - mkfs.vfat (dosfstools on Debian) or hdiutil (macOS)
#   - mcopy (mtools)  — used to write into the FAT image without mounting

OS_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$OS_DIR"

OUT="${OUT:-$OS_DIR/build/mars-usb.img}"
IMAGE_MB="${IMAGE_MB:-128}"
ESP_LABEL="${ESP_LABEL:-MARSOS}"

EFI_BIN="$OS_DIR/esp/EFI/BOOT/BOOTX64.EFI"
if [ ! -f "$EFI_BIN" ]; then
  echo "Building BOOTX64.EFI first..." >&2
  make all >&2
fi

need() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "error: missing tool '$1' — install $2" >&2
    exit 2
  fi
}
need sgdisk        "gdisk on Debian/Ubuntu, gptfdisk on Homebrew"
need mformat       "mtools on Debian/Ubuntu/Homebrew"
need mcopy         "mtools (same package as mformat)"

mkdir -p "$(dirname "$OUT")"
echo "[usb-image] building $OUT (${IMAGE_MB} MiB)" >&2
dd if=/dev/zero of="$OUT" bs=1048576 count="$IMAGE_MB" status=none

# GPT with a single ESP partition spanning sectors 2048..end-34 (standard GPT
# reserves the last 33 sectors for the secondary header).
sgdisk --zap-all "$OUT" >/dev/null
sgdisk --new=1:2048:0 \
       --typecode=1:ef00 \
       --change-name=1:"$ESP_LABEL" \
       "$OUT" >/dev/null

# Carve the ESP partition out into its own temp image, format it, copy the
# EFI binary in, then splice it back into the image at the right offset.
PART_OFFSET=$((2048 * 512))
TOTAL_BYTES=$((IMAGE_MB * 1024 * 1024))
PART_BYTES=$((TOTAL_BYTES - PART_OFFSET - (34 * 512)))

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
ESP_IMG="$WORK/esp.img"

dd if=/dev/zero of="$ESP_IMG" bs=512 count=$((PART_BYTES / 512)) status=none

# mformat parameters: -i image, -F FAT32, -v label
mformat -i "$ESP_IMG" -F -v "$ESP_LABEL" ::

mmd -i "$ESP_IMG" ::/EFI
mmd -i "$ESP_IMG" ::/EFI/BOOT
mcopy -i "$ESP_IMG" "$EFI_BIN" ::/EFI/BOOT/BOOTX64.EFI

# Splice the partition image back into the disk image at the correct offset.
dd if="$ESP_IMG" of="$OUT" bs=512 seek=2048 conv=notrunc status=none

echo "[usb-image] done: $OUT" >&2
echo "[usb-image] Flash with:" >&2
echo "  sudo dd if=$OUT of=/dev/sdX bs=4M status=progress conv=fdatasync" >&2
echo "(replace /dev/sdX with the actual USB device path; lsblk on Linux, diskutil list on macOS)" >&2
