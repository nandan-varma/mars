# Project Guidelines

## Code Style
- This workspace is a freestanding C UEFI OS under `os/`.
- Keep code compatible with strict flags used by `os/Makefile`: `-ffreestanding -fshort-wchar -fno-stack-protector -mno-red-zone -Wall -Wextra -Werror`.
- Use local UEFI ABI/types from `os/include/uefi.h` (`EFIAPI`, `EFI_STATUS`, `UINTN`, protocol structs).
- Follow existing style: module-static state (`g_*`), small helper functions, early-return guards, explicit bounds/clamp checks.

## Architecture
- Boot model is a single UEFI app (`BOOTX64.EFI`), entered via `efi_main` in `os/boot/efi_main.c`.
- Handoff uses `boot_info_t` (`os/include/boot_info.h`) to pass GOP/system/input protocol pointers into `kernel_main`.
- Main loop in `os/kernel/kernel.c`: poll input, drain queue, handle GUI events, conditional redraw, frame limit.
- Rendering is CPU software rasterization in `os/graphics/framebuffer.c` + font/text rendering; GUI is immediate-mode in `os/gui/gui.c`.

## Build and Test
- Build: `make -C os all`
- Run in QEMU: `make -C os run`
- Debug launch (gdb stub): `make -C os debug`
- GDB attach: `gdb os/esp/EFI/BOOT/BOOTX64.EFI` then `target remote :1234`
- Toolchain setup helper: `os/scripts/setup_macos_toolchain.sh`

## Project Conventions
- Keep scope minimal: no multitasking, SMP, ACPI parsing, filesystem, networking, sound, or GPU drivers.
- Input path is layered: UEFI Simple Pointer → UEFI Absolute Pointer → PS/2 fallback (`os/drivers/mouse_uefi.c`).
- Input events use fixed ring buffer (`INPUT_QUEUE_CAPACITY` in `os/drivers/input.c`); overflow drops events.
- `EXIT_BOOT_SERVICES` is compile-time gated in `os/boot/efi_main.c` (default disabled).
- Prefer targeted fixes in existing modules; do not introduce large new subsystems.

## Integration Points
- Protocol discovery and boot setup: `os/boot/efi_main.c`.
- Kernel orchestration/timing: `os/kernel/kernel.c`, `os/kernel/timer.c`.
- Graphics primitives/text: `os/graphics/framebuffer.c`, `os/graphics/font8x16.c`.
- GUI behavior/widgets: `os/gui/gui.c`.
- Shared interfaces: `os/include/*.h`.

## Security
- This is a single-address-space kernel prototype; no privilege separation exists.
- Preserve defensive checks present in code: pointer null checks, framebuffer bounds checks, coordinate clamping, fixed-size text/event buffers.
- Be careful with direct I/O port access in PS/2 code (`os/drivers/mouse_uefi.c`); keep loops bounded and non-blocking.
