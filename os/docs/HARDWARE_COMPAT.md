# MarsOS Hardware Compatibility

A live record of machines we have tried to boot Mars on. Add a row whenever you test a new machine, regardless of whether it works.

## How to test a new machine

1. Build a USB image: `./os/scripts/make_usb_image.sh`.
2. Flash to a USB stick: `sudo dd if=os/build/mars-usb.img of=/dev/sdX bs=4M status=progress conv=fdatasync` (see [RUNBOOK.md](RUNBOOK.md)).
3. In firmware setup, **disable Secure Boot** (until M3 signing infrastructure is set up) and enable USB boot.
4. Connect a USB-TTL adapter to the machine's COM1 header if present, otherwise note "no serial".
5. Boot. Record what happens.

## What to record

Be specific. "It worked" is not enough — note the firmware vendor/version, the boot path, what the last visible stage was, and any messages from serial.

## Compatibility matrix

| Date | Machine | Firmware | Boot | Last stage | Mouse | Keyboard | Notes |
|---|---|---|---|---|---|---|---|
| (template) | Vendor Model XYZ | AMI 1.23, BIOS 2024-01-01 | ❓ | — | — | — | Filled in by tester. Replace this row with a real result. |

Legend for **Boot**:
- ✅ — reached desktop (stage 200)
- ⚠️ — booted past `efi_main` but hung before stage 200; note last stage in next column
- ❌ — never reached `efi_main`; firmware rejected the binary or chose another boot entry

## Known working

*(empty until first real-hardware boot is recorded)*

## Known broken

| Date | Configuration | Symptom | Workaround |
|---|---|---|---|
| 2026-05-17 | QEMU OVMF + `EXIT_BOOT_SERVICES=1` | Boot reaches stage 90 (heap_init), then #UD (Invalid Opcode) before stage 100 (input_init). RIP ≈ 0xB0000, suggesting a function-pointer call into firmware code that was unmapped on exit. | Build with `EXIT_BOOT_SERVICES=0` (the default). Investigation tracked as a M3 follow-up — likely needs either (a) cached `EFI_SIMPLE_POINTER_PROTOCOL` function tables copied pre-exit, or (b) deferring the call until after all init has run, OR (c) a self-hosted PS/2 driver path that doesn't touch firmware. |

## Tested-but-flaky

*(empty until first real-hardware boot is recorded)*

## Common failure modes (predicted, to be confirmed)

- **Firmware reclaims Boot Services memory after `ExitBootServices`.** Some vendor BIOSes invalidate cached protocol pointers; block, network, and input drivers will then crash on first access. Pre-cache everything you need before `ExitBootServices` returns. Watch for stages 70 → 80 transitioning to a page fault.
- **GOP framebuffer base moves after exit.** Always capture `FrameBufferBase` and `Mode->Info` *before* the call; if the kernel still uses the GOP protocol after exit, the screen will go black or show garbage.
- **Legacy-free machines have no PS/2.** The current input fallback chain is PS/2 → AbsolutePointer → SimplePointer. Once Boot Services exit, the latter two disappear; on a legacy-free machine that leaves Mars with no input until M5's USB stack lands. Document the model as "no input post-exit" and use it for graphical-only testing.
- **Secure Boot enabled.** Until [sign_efi.sh](../scripts/sign_efi.sh) is paired with an enrolled cert, the firmware will silently refuse the binary. Disable Secure Boot or enroll the cert via your firmware's setup utility.
