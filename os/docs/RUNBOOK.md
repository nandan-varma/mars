# MarsOS Runbook

Operational instructions for booting Mars on real hardware, capturing diagnostics, and recovering from failures. Complements [HARDWARE_COMPAT.md](HARDWARE_COMPAT.md) (what works) and [observability.md](observability.md) (how to read the logs).

## Quick reference

| Want to | Do |
|---|---|
| Build everything | `make -C os ci` |
| Boot under QEMU (interactive) | `make -C os run` |
| Smoke-test under headless QEMU | `make -C os smoke-test` |
| Build a USB image | `./os/scripts/make_usb_image.sh` |
| Sign for Secure Boot | `MARS_KEY=… MARS_CERT=… ./os/scripts/sign_efi.sh` |

## Flashing to USB

1. **Build the USB image.**
   ```sh
   make -C os all
   ./os/scripts/make_usb_image.sh    # produces os/build/mars-usb.img
   ```

2. **Find the USB device.**
   - Linux: `lsblk` — usually `/dev/sdX`. Be sure it is not a system disk.
   - macOS: `diskutil list` — usually `/dev/diskN`. Unmount first with `diskutil unmountDisk /dev/diskN`.

3. **Flash.** Use a large block size for speed and `fdatasync` so the kernel waits for the write to land.
   ```sh
   sudo dd if=os/build/mars-usb.img of=/dev/sdX bs=4M status=progress conv=fdatasync
   ```
   On macOS, replace `/dev/sdX` with `/dev/rdiskN` (the raw device — much faster than `/dev/diskN`).

4. **Verify.** Read the first MiB back and diff to confirm the write took:
   ```sh
   sudo cmp -n 1048576 os/build/mars-usb.img /dev/sdX
   ```

## Booting on real hardware

1. In the target machine's firmware setup (usually F2, F10, F12, Del at power-on):
   - **Disable Secure Boot** unless you have already enrolled your `MARS_db.crt` (see [sign_efi.sh](../scripts/sign_efi.sh)).
   - Enable **USB boot** and put the USB stick first in the boot order, OR use the one-time boot menu (often F12).
   - **CSM/legacy**: leave it disabled. Mars is UEFI-only.
2. Insert the USB stick and reboot.
3. If the screen goes black after a moment, that is expected — the kernel is rendering its desktop to the GOP framebuffer. If it stays black, see "If something goes wrong" below.

## Capturing serial output

Real hardware does not always have a COM1 header, but most desktop motherboards and many low-cost mini-PCs do. A USB-to-TTL serial adapter (FT232, CP2102, CH340 — any of them) plugged into the header gives you the boot log.

Wiring (3.3 V TTL):
- Mars TX → adapter RX
- Mars RX → adapter TX (optional — Mars does not read serial yet)
- GND → GND
- **Do NOT connect VCC** unless you're sure it matches.

Capture on your dev machine:
```sh
# Linux: stty 115200 cs8 -cstopb -parenb -F /dev/ttyUSB0
screen /dev/ttyUSB0 115200
# Or, log to file:
stty -F /dev/ttyUSB0 raw 115200; cat /dev/ttyUSB0 | tee serial.log
```
```sh
# macOS:
screen /dev/cu.usbserial-* 115200
```

Add the captured `serial.log` to the [HARDWARE_COMPAT.md](HARDWARE_COMPAT.md) entry for the machine.

## If something goes wrong

The right diagnostic depends on how far boot got. See [observability.md](observability.md) for the output format.

| Symptom | Likely cause | First thing to try |
|---|---|---|
| Firmware says "no bootable device" | Image not written, wrong partition table, or `.EFI` in the wrong path | Re-run `make_usb_image.sh`; check ESP contains `/EFI/BOOT/BOOTX64.EFI` |
| Firmware boots, screen goes black, no serial output | Boot Services exit broke the framebuffer | Boot with `MARS_KEEP_BOOT_SERVICES=1` (default); record machine in [HARDWARE_COMPAT.md](HARDWARE_COMPAT.md) |
| Serial shows `[mars] efi_main:start` then halts | Initialization step failed silently | Find the last `[stage N]` line; cross-reference the stage table in [observability.md](observability.md) |
| `*** MARS PANIC ***` dump | Kernel hit an exception | The dump tells you the rest. Vector 0x0E = page fault; 0x0D = GP; 0xFF = explicit `panic_now` |
| Boot reaches `[stage 200]` but desktop never appears | Renderer didn't run, but kernel is alive | Check serial for app-spawn diag (`dom=0x0610`); plug in a USB keyboard, type things, see if input events show up |

## Recovering a bricked dev machine

Mars cannot brick a UEFI machine in the normal sense: nothing it does writes to firmware NVRAM (we don't use SetVariable at runtime). If a machine *seems* bricked after booting Mars:

1. Power off, remove USB stick, reboot. The boot order should fall through to the original OS.
2. If firmware setup is unreachable, do a CMOS clear (jumper on desktops; battery removal on laptops). This resets boot order.
3. Secure Boot key enrollment via `MOK` is reversible from your shim manager.

If a *vendor* firmware corrupts itself in response to something Mars does, file it as a hardware bug in the firmware vendor's tracker and add a "never boot Mars on this firmware version" row to [HARDWARE_COMPAT.md](HARDWARE_COMPAT.md).
