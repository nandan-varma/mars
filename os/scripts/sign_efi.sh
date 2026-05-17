#!/usr/bin/env bash
set -euo pipefail

# Sign BOOTX64.EFI for UEFI Secure Boot.
#
# This produces a signature using sbsign and a private key + cert pair you
# supply. The signature is embedded inline in the EFI binary (PE/COFF
# Authenticode), so the output is still a directly bootable .EFI.
#
# Typical Secure Boot workflow on a hobby OS:
#   1. Generate a self-signed cert (one-time):
#        openssl req -new -x509 -newkey rsa:2048 -nodes \
#          -keyout MARS_db.key -out MARS_db.crt \
#          -days 3650 -subj "/CN=MarsOS db/"
#   2. Sign with this script:
#        MARS_KEY=MARS_db.key MARS_CERT=MARS_db.crt ./sign_efi.sh
#   3. Enroll MARS_db.crt into the firmware's db (via firmware setup
#      utility, or with KeyTool.efi, or with the `mokutil` shim path).
#
# For CI: pass the key+cert via base64-encoded GitHub Actions secrets and
# decode them into temp files just before this script runs.
#
# Inputs (env vars):
#   IN         input EFI binary (default os/esp/EFI/BOOT/BOOTX64.EFI)
#   OUT        output signed binary (default $IN — sbsign writes in place
#              via a temp file)
#   MARS_KEY   path to private key (PEM)
#   MARS_CERT  path to certificate (PEM, x509)

OS_DIR="$(cd "$(dirname "$0")/.." && pwd)"
IN="${IN:-$OS_DIR/esp/EFI/BOOT/BOOTX64.EFI}"
OUT="${OUT:-$IN}"
MARS_KEY="${MARS_KEY:-}"
MARS_CERT="${MARS_CERT:-}"

if [ -z "$MARS_KEY" ] || [ -z "$MARS_CERT" ]; then
  echo "error: set MARS_KEY and MARS_CERT to point at your signing material" >&2
  exit 2
fi
if [ ! -f "$IN" ]; then
  echo "error: input EFI not found: $IN" >&2
  exit 2
fi
if [ ! -f "$MARS_KEY" ] || [ ! -f "$MARS_CERT" ]; then
  echo "error: MARS_KEY ($MARS_KEY) or MARS_CERT ($MARS_CERT) does not exist" >&2
  exit 2
fi

if ! command -v sbsign >/dev/null 2>&1; then
  echo "error: sbsign not found — install sbsigntool (Debian) or sbsigntools (Homebrew)" >&2
  exit 2
fi

echo "[sign] signing $IN" >&2
sbsign --key "$MARS_KEY" --cert "$MARS_CERT" --output "$OUT" "$IN"

if command -v sbverify >/dev/null 2>&1; then
  echo "[sign] verifying $OUT" >&2
  sbverify --cert "$MARS_CERT" "$OUT"
fi

echo "[sign] done: $OUT" >&2
