# Project Guidelines

## Code Style
- Workspace is freestanding UEFI C under `os/`; preserve `os/Makefile` constraints (`-ffreestanding -fshort-wchar -fno-stack-protector -mno-red-zone -Wall -Wextra -Werror`).
- Use local ABI/types from `os/include/uefi.h` (`EFIAPI`, `EFI_STATUS`, `UINTN`, protocol structs); avoid libc assumptions.
- Follow module-static `g_*` state + small helpers + early-return guards (examples: `os/kernel/event_channel.c`, `os/kernel/memory.c`).
- Keep all copies/payloads bounded with explicit capacities (examples: `os/kernel/event_bus.c`, `os/gui/wm_input.c`, `os/drivers/input.c`).

## Architecture
- Boot handoff: `os/boot/efi_main.c` builds `boot_info_t` then enters `kernel_main`.
- Boot Services stay active at runtime via `platform_context_t` (`os/kernel/platform.c`, `os/include/platform.h`).
- Kernel bring-up in `os/kernel/kernel.c` is table-driven with preserved stage markers and ordering.
- Scheduler is cooperative round-robin (`os/kernel/scheduler.c`), with optional timer-preemptive mode toggle.
- WM behavior is implemented in split modules (`os/gui/wm_state.c`, `wm_input.c`, `wm_render.c`, `wm_menu.c`) with API in `os/include/wm.h`.

## Build and Test
- Build: `make -C os all`
- Run: `make -C os run`
- Debug (gdb stub, halted): `make -C os debug`
- Host contract tests: `make -C os test-host`
- Full guardrail pass: `make -C os clean && make -C os all && make -C os test-host`
- Additional aliases (`run-gui`, `run-fs`, `run-apps`) currently route to `run`.
- GDB attach: `gdb os/esp/EFI/BOOT/BOOTX64.EFI` then `target remote :1234`
- UEFI shell boot command: `FS0:\EFI\BOOT\BOOTX64.EFI`
- Toolchain helper: `os/scripts/setup_macos_toolchain.sh`

## Project Conventions
- Keep `os/Makefile` compatibility with both MinGW PE and ELF+objcopy toolchain paths.
- Input is event-bus based: `os/drivers/input.c` publishes channel events; WM/apps consume via `os/kernel/event_bus.c`.
- Mouse source priority is PS/2 → Absolute Pointer → Simple Pointer (`os/drivers/mouse_uefi.c`, `mouse_ps2.c`, `mouse_absolute.c`, `mouse_simple.c`); preserve bounded polling loops.
- App launch is event-driven: WM publishes `EVENT_CODE_APP_LAUNCH_REQUEST`; app manager consumes `EVENT_CHANNEL_SYSTEM` (`os/gui/wm_state.c`, `os/kernel/app.c`).
- Event bus is bounded/non-blocking; preserve queue/drop semantics and policies (`os/kernel/event_bus.c`, `os/kernel/event_channel.c`).
- Keep public headers stable while refactoring internals (`os/include/wm.h`, `os/include/app.h`, `os/include/event_bus.h`, `os/include/memory.h`, `os/include/vm.h`).
- Prefer targeted fixes in existing modules; avoid large subsystem additions unless explicitly requested.

## Integration Points
- Boot protocol discovery + handoff: `os/boot/efi_main.c`, `os/include/boot_info.h`
- Platform bridge consumed by subsystems: `os/kernel/platform.c`, `os/include/platform.h`
- Desktop + app seams: `os/gui/wm_state.c`, `os/gui/wm_input.c`, `os/gui/wm_render.c`, `os/gui/wm_menu.c`, `os/kernel/app.c`, `os/kernel/app_registry.c`, `os/kernel/app_instance.c`, `os/kernel/app_console_cmd.c`
- Event/memory/vm seams: `os/kernel/event_bus.c`, `os/kernel/event_channel.c`, `os/kernel/event_packet.c`, `os/kernel/memory.c`, `os/kernel/memory_pages.c`, `os/kernel/memory_freelist.c`, `os/kernel/vm.c`, `os/kernel/vm_builder.c`
- Shared interfaces live in `os/include/*.h`

## Security
- Single-address-space prototype: isolation is cooperative; do not assume hard process boundaries (`os/kernel/process.c`, `os/kernel/syscall.c`).
- Syscall event publishing to `EVENT_CHANNEL_SYSTEM` requires capabilities; keep that gate intact in `os/kernel/syscall.c`.
- Preserve defensive checks already in use: null checks, alignment/payload-size checks, queue-full handling, framebuffer bounds checks.
- Keep PS/2 I/O waits and polling budgets bounded (`os/drivers/mouse_uefi.c`, `os/drivers/mouse_ps2.c`).
- `OVMF_VARS.fd` is optional in run/debug; without it, firmware variable persistence is reduced.
