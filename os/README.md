# MarsOS (UEFI x86_64 desktop prototype)

## Architecture Overview
- **Target**: x86_64, UEFI firmware, GOP framebuffer.
- **Boot model**: UEFI `BOOTX64.EFI` with **Boot Services ON** runtime contract.
- **Kernel model**: single-address-space cooperative multitasking with process/task abstractions.
- **Graphics**: CPU software rendering with framebuffer present.
- **Desktop**: WM-based path (`gui/wm.c`) with overlapping windows, taskbar, start menu.
- **Input**: event-bus routed keyboard/mouse via UEFI + bounded PS/2 fallback.
- **Scheduler**: cooperative round-robin (no preemption; task priority is stored metadata).

### Intentional scope limits
- No hardware process isolation yet (prototype capabilities only).
- No GPU acceleration, networking, audio, or full storage stack.
- No preemptive scheduler/context switching.

### Comparison (concise)
- **Linux/Windows**: full scheduler, MMU process isolation, extensive drivers, storage/network stacks.
- **MINIX**: microkernel + message-passing architecture with richer process/services model.
- **MarsOS MVP**: educational monolithic prototype focused on UEFI boot + evented services + software desktop stack.

## Memory Layout (current)

```
+-------------------------------+  High Address
| UEFI Runtime Regions          |
+-------------------------------+
| GOP Framebuffer               |  (physical VRAM mapping from UEFI)
+-------------------------------+
| UEFI Boot Services Regions    |  (active runtime dependency)
+-------------------------------+
| MarsOS .text / .data / .bss   |  (BOOTX64.EFI image)
+-------------------------------+
| UEFI image load region         |
+-------------------------------+
| Reserved / firmware regions    |
+-------------------------------+  Low Address
```

## Boot Flow
1. Firmware loads `esp/EFI/BOOT/BOOTX64.EFI`.
2. `efi_main` locates GOP, keyboard, and pointer protocols via Boot Services.
3. Framebuffer metadata is packed into `boot_info_t`.
4. Boot info (`boot_info_t`) is handed to `kernel_main`.
5. Kernel brings up: platform/diag/interrupts/event bus/timer/scheduler/process.
6. Then memory/framebuffer/vm/heap, syscall/input/WM/VFS/apps.
7. Supervisor and service tasks run in cooperative scheduler loop.

## Directory Structure

```
os/
├── boot/                 # UEFI entry point and protocol discovery
├── kernel/               # bring-up, scheduler, process, events, apps, syscall, vfs
├── drivers/              # keyboard/mouse drivers + event queue
├── graphics/             # framebuffer primitives + bitmap font
├── gui/                  # active WM desktop implementation
├── lib/                  # tiny freestanding helpers
├── include/              # shared headers and interfaces
├── docs/                 # design notes
├── scripts/              # helper scripts (ESP image output)
├── esp/EFI/BOOT/         # generated BOOTX64.EFI location
├── linker.ld             # minimal linker script (future freestanding path)
└── Makefile              # build/run/debug automation
```

## Build Instructions

### 1) Host prerequisites (macOS)
- `x86_64-w64-mingw32-gcc` (preferred EFI linker path)
- `x86_64-elf-gcc` (optional fallback toolchain)
- `qemu-system-x86_64`
- `make`

Example Homebrew baseline:
- `brew install qemu mingw-w64 x86_64-elf-gcc x86_64-elf-binutils`

### 2) Configure paths if needed
Default assumptions in `Makefile`:
- `OVMF_CODE=/opt/homebrew/Cellar/qemu/.../share/qemu/edk2-x86_64-code.fd` (auto-detected)

Override on command line if different:
- `make OVMF_CODE=/path/to/edk2-x86_64-code.fd`

### 3) Build
- `make -C os all`

Output:
- `os/esp/EFI/BOOT/BOOTX64.EFI`

## QEMU Instructions

### Normal boot
- `make -C os run`

Notes:
- `Makefile` uses `-machine pc` with no USB pointer override, favoring legacy PS/2 input routing.
- Mouse polling priority is `PS/2` first, then `AbsolutePointer`, then `SimplePointer` fallback.
- If the cursor seems stuck, click once inside the QEMU window to capture pointer focus.

### Expected behavior
- Desktop with taskbar/start menu and multiple app windows.
- App launch requests flow through system-channel events to app manager.
- WM debug overlay shows FPS/process/task plus event-drop counters.

## Debugging Instructions

### Serial output debugging
- `make -C os run` already uses `-serial stdio`.
- For input debugging, temporarily print protocol-detection flags from `efi_main`.

### GDB remote debugging
1. Start VM halted:
   - `make -C os debug`
2. In another terminal:
   - `gdb os/esp/EFI/BOOT/BOOTX64.EFI`
   - `target remote :1234`
   - set breakpoints on `efi_main` or `kernel_main`.

### Inspect framebuffer correctness
- Validate pixel writes by drawing fixed test rectangles and text at known coordinates.
- Check pitch handling by drawing near right edge and bottom edge.

### Common boot failure reasons
- Wrong OVMF path.
- Missing `BOOTX64.EFI` in `esp/EFI/BOOT`.
- Missing MinGW toolchain (`x86_64-w64-mingw32-gcc`).
- QEMU firmware missing x86_64 code image (`edk2-x86_64-code.fd`).

## Runtime services
- `input-service`: polls UEFI/PS2 sources and publishes input events.
- `wm-service`: consumes input channels and manages focus/window interactions.
- `render-service`: renders desktop when WM reports dirty state.
- `service-supervisor`: relaunches managed services when they terminate.
- `app-manager`: consumes `EVENT_CHANNEL_SYSTEM` launch requests and spawns app instances.

## Current Limitations
- `memory_release_pages` is currently a stub and does not reclaim pages.
- Process/task tables are bounded (`MAX_PROCESSES`/`MAX_TASKS`) and rely on slot reuse.

## Testing Strategy
- Build gate: `make -C os all` after each refactor phase.
- Runtime smoke: `make -C os run` and validate launch/focus/close/input flows.
- Debug gate: `make -C os debug` + GDB attach for scheduler/event diagnostics.

## Next Architecture Opportunities
- Dirty-region present path for reduced render cost.
- Queue backpressure policies and per-channel QoS.
- Stronger process isolation model once VM context switching is introduced.
