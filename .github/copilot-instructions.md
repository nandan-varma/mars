# Project Guidelines

## Code Style
- Workspace is a freestanding C UEFI OS under `os/`; keep code compatible with `os/Makefile` flags: `-ffreestanding -fshort-wchar -fno-stack-protector -mno-red-zone -Wall -Wextra -Werror`.
- Use local ABI/types from `os/include/uefi.h` (`EFIAPI`, `EFI_STATUS`, `UINTN`, protocol structs); avoid libc assumptions.
- Match existing style: module-static `g_*` state, small helpers, early-return guards, explicit bounds/clamp checks, fixed-size buffers.

## Architecture
- Boot entry is `efi_main` in `os/boot/efi_main.c`, handing `boot_info_t` into `kernel_main`.
- Runtime bring-up is in `os/kernel/kernel.c`: platform + memory/vm/heap + interrupts/timer + event bus + process/scheduler + input/wm/apps + vfs.
- Scheduling is cooperative round-robin in `os/kernel/scheduler.c` (no preemptive context switch yet).
- Active desktop path is WM/compositor in `os/gui/wm.c`; `os/gui/gui.c` exists but is not wired into current build/runtime.
- Rendering is software rasterization with optional backbuffer + blit present in `os/graphics/framebuffer.c`.

## Build and Test
- Build: `make -C os all`
- Run: `make -C os run`
- Debug (gdb stub, halted): `make -C os debug`
- GDB attach: `gdb os/esp/EFI/BOOT/BOOTX64.EFI` then `target remote :1234`
- UEFI shell boot command: `FS0:\EFI\BOOT\BOOTX64.EFI`
- Toolchain helper: `os/scripts/setup_macos_toolchain.sh`

## Project Conventions
- Build auto-detects MinGW PE path when available; keep changes compatible with both MinGW and ELF+objcopy paths in `os/Makefile`.
- Input is event-bus based: `os/drivers/input.c` publishes channel events; WM/apps consume via `os/kernel/event_bus.c`.
- Mouse path priority is PS/2 → Absolute Pointer → Simple Pointer in `os/drivers/mouse_uefi.c`; keep direct I/O waits bounded.
- `EXIT_BOOT_SERVICES` in `os/boot/efi_main.c` is compile-time gated and defaults to disabled.
- Keep fixes targeted to existing modules; avoid large subsystem additions unless requested.

## Integration Points
- Boot protocol discovery + memory map handoff: `os/boot/efi_main.c`
- Memory and paging scaffold: `os/kernel/memory.c`, `os/kernel/vm.c`
- Desktop/window management + app window content: `os/gui/wm.c`, `os/kernel/app.c`
- Event routing and process queues: `os/kernel/event_bus.c`, `os/kernel/process.c`
- Shared interfaces: `os/include/*.h`

## Security
- Single-address-space prototype: no strong isolation; capability checks are lightweight (`process/syscall` path).
- Preserve current defensive patterns: null checks, payload-size checks, queue capacity checks, framebuffer bounds checks.
- Be careful with PS/2 port I/O loops in `os/drivers/mouse_uefi.c`; keep them bounded and non-blocking.
- `OVMF_VARS.fd` is optional in run/debug; without it, firmware variable persistence is reduced.
