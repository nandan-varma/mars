# Project Guidelines

## Code Style
- Workspace is a freestanding C UEFI OS under `os/`; keep compatibility with `os/Makefile` flags (`-ffreestanding -fshort-wchar -fno-stack-protector -mno-red-zone -Wall -Wextra -Werror`).
- Use local ABI/types from `os/include/uefi.h` (`EFIAPI`, `EFI_STATUS`, `UINTN`, protocol structs); avoid libc assumptions.
- Match existing style: module-static `g_*` state, small helpers, early-return guards, explicit bounds/clamp checks, fixed-size buffers.
- Keep copy/payload logic bounded (examples: `os/kernel/event_bus.c`, `os/drivers/input.c`, `os/gui/wm.c`).

## Architecture
- Boot entry is `efi_main` in `os/boot/efi_main.c`, which prepares `boot_info_t` and calls `kernel_main`.
- Runtime keep Boot Services active and passes handles through `platform_context_t` (`os/kernel/platform.c`, `os/include/platform.h`).
- Bring-up order in `os/kernel/kernel.c` is table-driven: platform/diag/interrupts/event bus/timer/scheduler/process, then memory/framebuffer/vm/heap, then syscall/input/wm/vfs/apps.
- Scheduler is cooperative round-robin in `os/kernel/scheduler.c` (no preemptive context switching; task `priority` is metadata today).
- Active desktop path is WM-based (`os/gui/wm.c` façade over `wm_state.c`/`wm_input.c`/`wm_render.c`/`wm_menu.c`), rendering via software rasterization in `os/graphics/framebuffer.c`.

## Build and Test
- Build: `make -C os all`
- Run: `make -C os run`
- Debug (gdb stub, halted): `make -C os debug`
- Additional aliases (`run-gui`, `run-fs`, `run-apps`) currently route to `run`.
- GDB attach: `gdb os/esp/EFI/BOOT/BOOTX64.EFI` then `target remote :1234`
- UEFI shell boot command: `FS0:\EFI\BOOT\BOOTX64.EFI`
- Toolchain helper: `os/scripts/setup_macos_toolchain.sh`

## Project Conventions
- Keep `os/Makefile` compatibility with both MinGW PE and ELF+objcopy toolchain paths.
- Input is event-bus based: `os/drivers/input.c` publishes channel events; WM/apps consume via `os/kernel/event_bus.c`.
- Mouse source priority is PS/2 → Absolute Pointer → Simple Pointer with strategy files (`os/drivers/mouse_ps2.c`, `mouse_absolute.c`, `mouse_simple.c`) orchestrated by `os/drivers/mouse_uefi.c`; preserve bounded polling loops.
- App launch is event-driven: WM publishes `EVENT_CODE_APP_LAUNCH_REQUEST` on `EVENT_CHANNEL_SYSTEM`; app manager in `os/kernel/app.c` consumes and launches.
- Event bus is bounded/non-blocking; preserve queue capacity semantics and drop telemetry in `os/kernel/event_bus.c` (internals in `event_channel.c`/`event_packet.c`).
- Keep public headers stable while refactoring internals (`os/include/wm.h`, `os/include/app.h`, `os/include/event_bus.h`, `os/include/memory.h`, `os/include/vm.h`).
- Prefer targeted fixes in existing modules; avoid large subsystem additions unless explicitly requested.

## Integration Points
- Boot protocol discovery + handoff: `os/boot/efi_main.c`, `os/include/boot_info.h`
- Platform bridge consumed by subsystems: `os/kernel/platform.c`, `os/include/platform.h`
- Desktop/window management + app content: `os/gui/wm.c`, `os/gui/wm_state.c`, `os/gui/wm_input.c`, `os/gui/wm_render.c`, `os/gui/wm_menu.c`, `os/kernel/app.c`, `os/kernel/app_registry.c`, `os/kernel/app_instance.c`, `os/kernel/app_console_cmd.c`
- Event routing and process/task seams: `os/kernel/event_bus.c`, `os/kernel/event_channel.c`, `os/kernel/event_packet.c`, `os/kernel/process.c`, `os/kernel/scheduler.c`
- Shared interfaces live in `os/include/*.h`

## Security
- Single-address-space prototype: isolation is cooperative; do not assume hard process boundaries (`os/kernel/process.c`, `os/kernel/syscall.c`).
- Syscall event publishing to `EVENT_CHANNEL_SYSTEM` requires capabilities; keep that gate intact in `os/kernel/syscall.c`.
- Preserve defensive checks already in use: null checks, payload-size checks, queue-full handling, framebuffer bounds checks.
- Keep PS/2 I/O waits bounded/non-blocking in `os/drivers/mouse_uefi.c`.
- `OVMF_VARS.fd` is optional in run/debug; without it, firmware variable persistence is reduced.
