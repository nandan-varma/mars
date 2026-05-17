#!/usr/bin/env bash
set -euo pipefail

# Headless boot smoke test.
#
# Boots BOOTX64.EFI under QEMU with no display, captures the COM1 serial
# stream to a file, then asserts that the kernel reached the final
# initialization stage (200 — services_and_apps) before the timeout. This
# is the CI gate that catches PRs that break boot.
#
# Tunables (env vars):
#   TIMEOUT_SECONDS  how long to let the VM run (default 30)
#   SENTINEL         grep pattern that proves boot succeeded (default
#                    "stage 200")
#   QEMU             qemu binary (default qemu-system-x86_64)
#   OVMF_CODE        path to OVMF code firmware (auto-detected)
#   ARTIFACTS_DIR    where to write serial.log (default os/build/smoke)

OS_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$OS_DIR"

TIMEOUT_SECONDS="${TIMEOUT_SECONDS:-30}"
SENTINEL="${SENTINEL:-stage 200}"
QEMU="${QEMU:-qemu-system-x86_64}"
ARTIFACTS_DIR="${ARTIFACTS_DIR:-$OS_DIR/build/smoke}"

if [ ! -x "$(command -v "$QEMU")" ]; then
  echo "error: $QEMU not on PATH" >&2
  exit 2
fi

if [ ! -f "$OS_DIR/esp/EFI/BOOT/BOOTX64.EFI" ]; then
  echo "Building BOOTX64.EFI before smoke test..." >&2
  make all >&2
fi

# OVMF discovery — Debian bookworm only ships the 4M variant, so try that
# first. Then fall back to the legacy 2M paths and Homebrew layouts.
if [ -z "${OVMF_CODE:-}" ]; then
  for candidate in \
    /usr/share/OVMF/OVMF_CODE_4M.fd \
    /usr/share/OVMF/OVMF_CODE.fd \
    /usr/share/edk2-ovmf/OVMF_CODE.fd \
    /usr/share/edk2/ovmf/OVMF_CODE.fd \
    /usr/share/qemu/OVMF.fd \
    /opt/homebrew/share/edk2/ovmf/OVMF_CODE.fd \
    /opt/homebrew/Cellar/qemu/*/share/qemu/edk2-x86_64-code.fd; do
    # shellcheck disable=SC2086
    set -- $candidate
    if [ -f "$1" ]; then
      OVMF_CODE="$1"
      break
    fi
  done
fi

if [ -z "${OVMF_CODE:-}" ] || [ ! -f "$OVMF_CODE" ]; then
  echo "error: no OVMF_CODE found; set OVMF_CODE=/path/to/OVMF_CODE.fd" >&2
  exit 2
fi

mkdir -p "$ARTIFACTS_DIR"
SERIAL_LOG="$ARTIFACTS_DIR/serial.log"
: > "$SERIAL_LOG"

# Ensure the disk image exists (Makefile's all target builds it).
if [ ! -f "$OS_DIR/build/mars.img" ]; then
  make disk-image >&2
fi

# Use a private copy of the disk image and ESP so an interactive `make run`
# session can't deadlock us on QEMU's image lock and vice versa.
PRIVATE_IMG="$ARTIFACTS_DIR/mars.img"
PRIVATE_ESP="$ARTIFACTS_DIR/esp"
cp "$OS_DIR/build/mars.img" "$PRIVATE_IMG"
rm -rf "$PRIVATE_ESP"
mkdir -p "$PRIVATE_ESP"
cp -R "$OS_DIR/esp/." "$PRIVATE_ESP/"

echo "[smoke] booting under $QEMU (timeout ${TIMEOUT_SECONDS}s)" >&2
echo "[smoke] serial log: $SERIAL_LOG" >&2
echo "[smoke] sentinel:  '$SENTINEL'" >&2

# -no-reboot so a triple fault halts instead of looping forever.
# nographic puts everything we care about onto the serial file already.
# Build a proper bootable disk image (GPT + ESP partition) via the same
# script the M3 USB workflow uses. OVMF requires a partition table to
# recognize a FAT volume as an ESP; a raw FAT image without one fails to
# load Boot0001 and falls through to PXE.
#
# Falls back to QEMU's flaky vvfat backend if the partitioning tools
# (sgdisk + mtools) aren't installed — vvfat works on macOS but
# SIGABRTs on Ubuntu CI, hence the prefer-real-image path.
DISK_IMG="$ARTIFACTS_DIR/smoke-disk.img"
if command -v sgdisk >/dev/null 2>&1 && command -v mformat >/dev/null 2>&1; then
  OUT="$DISK_IMG" IMAGE_MB=64 "$OS_DIR/scripts/make_usb_image.sh" >&2
  ESP_DRIVE_ARG=("-drive" "format=raw,file=$DISK_IMG,if=ide")
  echo "[smoke] using bootable disk image at $DISK_IMG" >&2
else
  ESP_DRIVE_ARG=("-drive" "format=raw,file=fat:rw:$PRIVATE_ESP")
  echo "[smoke] sgdisk + mtools missing — falling back to QEMU vvfat (less reliable on Linux CI)" >&2
fi

# Force TCG accel — GitHub runners don't have KVM and QEMU sometimes asserts
# on auto-detection. Capture QEMU's own stderr to qemu.stderr so an abort
# like SIGABRT leaves a trail.
"$QEMU" \
  -machine q35,accel=tcg -m 512M \
  -display none \
  -no-reboot \
  -drive "if=pflash,format=raw,readonly=on,file=$OVMF_CODE" \
  "${ESP_DRIVE_ARG[@]}" \
  -serial "file:$SERIAL_LOG" \
  -monitor none \
  2> "$ARTIFACTS_DIR/qemu.stderr" \
  &
QEMU_PID=$!

# Watch the log; break out as soon as the sentinel appears.
deadline=$(( $(date +%s) + TIMEOUT_SECONDS ))
status=1
while [ "$(date +%s)" -lt "$deadline" ]; do
  if grep -q -- "$SENTINEL" "$SERIAL_LOG" 2>/dev/null; then
    status=0
    break
  fi
  if ! kill -0 "$QEMU_PID" 2>/dev/null; then
    # QEMU exited; check log one more time
    if grep -q -- "$SENTINEL" "$SERIAL_LOG" 2>/dev/null; then
      status=0
    fi
    break
  fi
  sleep 1
done

if kill -0 "$QEMU_PID" 2>/dev/null; then
  kill "$QEMU_PID" 2>/dev/null || true
  # Give it a beat to clean up; then SIGKILL if needed.
  sleep 1
  kill -9 "$QEMU_PID" 2>/dev/null || true
fi
wait "$QEMU_PID" 2>/dev/null || true

echo "---- serial.log (tail) ----" >&2
tail -n 40 "$SERIAL_LOG" >&2 || true
echo "---- end serial.log ----" >&2

if [ -s "$ARTIFACTS_DIR/qemu.stderr" ]; then
  echo "---- qemu.stderr ----" >&2
  cat "$ARTIFACTS_DIR/qemu.stderr" >&2 || true
  echo "---- end qemu.stderr ----" >&2
fi

if [ "$status" -eq 0 ]; then
  echo "[smoke] PASS — found sentinel '$SENTINEL'" >&2
else
  echo "[smoke] FAIL — sentinel '$SENTINEL' not seen in ${TIMEOUT_SECONDS}s" >&2
fi
exit "$status"
