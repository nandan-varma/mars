# Project Guidelines

This file contains guidelines for agentic coding agents working on the Mars UEFI OS project. It covers build/lint/test commands, code style guidelines, and other conventions to ensure consistency and quality.

## Build/Lint/Test Commands

### Build Commands
- **Full build**: `make -C os all` - Builds the entire OS, user apps, and disk image.
- **EFI binary only**: `make -C os esp` - Builds only the BOOTX64.EFI file.
- **User apps**: `make -C os user-apps` - Creates basic app manifests in build/apps/.
- **Disk image**: `make -C os disk-image` - Creates a 64MB disk image.

### Run Commands
- **Run in QEMU**: `make -C os run` - Launches the OS in QEMU with OVMF firmware.
- **Run with GUI**: `make -C os run-gui` - Alias for run (currently no special GUI handling).
- **Run with FS**: `make -C os run-fs` - Alias for run.
- **Run with apps**: `make -C os run-apps` - Alias for run.

### Debug Commands
- **Debug with GDB**: `make -C os debug` - Runs QEMU with GDB stub; attach via `gdb os/esp/EFI/BOOT/BOOTX64.EFI` then `target remote :1234`.
- **UEFI shell boot**: `FS0:\EFI\BOOT\BOOTX64.EFI` - Command for booting in UEFI shell.

### Test Commands
- **All host tests**: `make -C os test-host` - Runs all contract tests on host.
- **Single test run**: To run an individual test, build it first with `make -C os test-host` (which builds all), then execute the binary directly, e.g., `./os/build/tests/test_event_bus_contract`.
- **Available tests**:
  - `test_event_bus_contract`: Validates event bus contracts.
  - `test_scheduler_fairness`: Tests scheduler fairness.
  - `test_process_slot_reuse`: Tests process slot reuse.
  - `test_input_bounds`: Tests input bounds checking.
  - `test_heap`: Tests heap allocation.
  - `test_freelist`: Tests memory freelist.
  - `test_vm_builder`: Tests VM builder.
  - `test_vm`: Tests virtual memory.
  - `test_input_security_fuzz`: Fuzz tests input security.
  - `test_process_stress`: Stress tests processes.
  - `test_performance_baseline`: Performance baseline.
  - `test_memory_safety_expanded`: Memory safety tests.
  - `test_security_audit_fixes`: Security audit fixes.
- **Full guardrail pass**: `make -C os clean && make -C os all && make -C os test-host` - Clean build and test.

### Lint Commands
- **No dedicated linter**: Code must compile with `-Wall -Wextra -Werror`. Run `make -C os all` to check for warnings/errors.
- **Toolchain setup**: Run `os/scripts/setup_macos_toolchain.sh` for macOS dependencies.

### Other Commands
- **Clean**: `make -C os clean` - Removes build artifacts.
- **Toolchain detection**: Makefile auto-detects compilers (x86_64-elf-gcc, clang, MinGW) and tools.

## Code Style Guidelines

### Language and Environment
- **Language**: Freestanding UEFI C (no libc assumptions).
- **Compiler flags**: Must use `-ffreestanding -fshort-wchar -fno-stack-protector -mno-red-zone -Wall -Wextra -Werror`.
- **ABI/Types**: Use UEFI types from `os/include/uefi.h` (e.g., `EFIAPI`, `EFI_STATUS`, `UINTN`, protocol structs). Avoid standard C library functions.

### Naming Conventions
- **Functions**: Lowercase with underscores, e.g., `event_bus_init()`, `input_publish_event()`.
- **Variables**: Lowercase with underscores for locals, e.g., `packet_size`. Prefix globals with `g_`, e.g., `g_state`.
- **Constants**: Uppercase with underscores, e.g., `EVENT_PAYLOAD_BYTES`.
- **Types/Structs**: Lowercase with underscores, e.g., `event_packet_t`. Use `_t` suffix for typedefs.
- **Enums**: Uppercase with underscores, e.g., `EVENT_CHANNEL_INPUT`.
- **Files**: Lowercase with underscores, matching module names, e.g., `event_bus.c`.

### Code Structure
- **Modules**: Use module-static state with `g_*` globals + small helper functions.
- **Function guards**: Start functions with null checks and early returns (e.g., `if (packet == NULL) return FALSE;`).
- **Bounded operations**: All copies/payloads must be bounded with explicit capacities (e.g., check `payload_size <= EVENT_PAYLOAD_BYTES`).
- **Error handling**: Return `BOOLEAN` or `EFI_STATUS`; use early returns for failures. Log errors via `diag_log` with codes.
- **Memory management**: Use bounded allocations; track outstanding pages. Avoid unbounded loops.

### Imports and Includes
- **Local headers first**: Include local headers before internal ones, e.g., `#include "event_bus.h"` then `#include "internal/event_channel.h"`.
- **No libc**: Avoid `<stdio.h>`, `<stdlib.h>`, etc.; use UEFI equivalents where available.
- **Header guards**: Use `#pragma once` or traditional guards in headers.

### Formatting
- **Indentation**: 4 spaces (no tabs).
- **Line length**: Aim for <100 characters; break long lines.
- **Braces**: Opening brace on same line, e.g., `if (cond) {`.
- **Spacing**: Space after keywords (`if`, `for`), around operators, no space in function calls like `func(arg)`.
- **Comments**: Use `//` for single-line comments. Add comments for fixes or complex logic. No block comments `/* */` unless necessary.

### Types and Safety
- **Typedefs**: Use UEFI types; define custom types with `_t` suffix.
- **Alignment**: Ensure proper alignment for UEFI structs.
- **Security**: Implement capability checks for privileged operations. Preserve defensive checks (null, bounds, alignment).
- **Performance**: Prefer word-aligned copies (64-bit) over byte loops for large structs.

### Best Practices
- **Avoid large additions**: Prefer targeted fixes in existing modules.
- **Testing**: Add host tests for new features; ensure tests pass before commit.
- **Documentation**: Update this file for new conventions.
- **Commits**: Follow existing commit style (focus on why, not what).

## Architecture
- Boot handoff: `os/boot/efi_main.c` builds `boot_info_t` then enters `kernel_main`.
- Boot Services stay active at runtime via `platform_context_t` (`os/kernel/platform.c`, `os/include/platform.h`).
- Kernel bring-up in `os/kernel/kernel.c` is table-driven with preserved stage markers and ordering.
- Scheduler is cooperative round-robin (`os/kernel/scheduler.c`), with optional timer-preemptive mode toggle.
- WM behavior is implemented in split modules (`os/gui/wm_state.c`, `wm_input.c`, `wm_render.c`, `wm_menu.c`) with API in `os/include/wm.h`.

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