# MarsOS (Minimal UEFI x86_64 GUI OS)

## Architecture Overview
- **Target**: x86_64, UEFI firmware, GOP framebuffer.
- **Boot model chosen**: **pure UEFI** (no GRUB) using `BOOTX64.EFI`.
- **Why this choice**: minimal bring-up path, direct access to GOP + UEFI keyboard/mouse protocols.
- **Kernel model**: single address space, no multitasking/SMP, no dynamic allocator.
- **Graphics**: CPU software rendering into linear framebuffer.
- **GUI**: one non-overlapping window, title bar, close button visual only, one button, one text field.
- **Input**: UEFI `SimpleTextInputEx` + `SimplePointer` with polling and event queue.

### What was intentionally avoided
- No ACPI parsing, no USB stack, no networking, no sound, no filesystem, no compositor.
- No GPU drivers; all drawing is software.

### Comparison (concise)
- **Linux/Windows**: full scheduler, MMU process isolation, extensive drivers, storage/network stacks.
- **MINIX**: microkernel + message-passing architecture with richer process/services model.
- **MarsOS MVP**: educational monolithic prototype focused on boot + graphics + input fundamentals.

## Memory Layout Diagram

```
+-------------------------------+  High Address
| UEFI Runtime Regions          |
+-------------------------------+
| GOP Framebuffer               |  (physical VRAM mapping from UEFI)
+-------------------------------+
| UEFI Boot Services Regions    |  (present unless EXIT_BOOT_SERVICES=1)
+-------------------------------+
| MarsOS .text / .data / .bss   |  (BOOTX64.EFI image)
+-------------------------------+
| UEFI image load region         |
+-------------------------------+
| Reserved / firmware regions    |
+-------------------------------+  Low Address
```

## Boot Flow Explanation
1. Firmware loads `esp/EFI/BOOT/BOOTX64.EFI`.
2. `efi_main` initializes UEFI library, locates GOP, keyboard, and pointer protocols.
3. Framebuffer metadata is packed into `boot_info_t`.
4. Optional phase-1 path: `ExitBootServices` can be enabled with `-DEXIT_BOOT_SERVICES=1`.
5. Control transfers to `kernel_main`.
6. Kernel initializes framebuffer, input queue/drivers, GUI, and timer.
7. Main loop: poll input → dispatch events → update GUI state → redraw → frame-limit.

## Directory Structure

```
os/
├── boot/                 # UEFI entry point and protocol discovery
├── kernel/               # kernel_main loop and timer
├── drivers/              # keyboard/mouse drivers + event queue
├── graphics/             # framebuffer primitives + bitmap font
├── gui/                  # window/button/textfield rendering and event handling
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
- `Makefile` adds `-device qemu-xhci -device usb-tablet` by default for stable mouse input.
- If the cursor seems stuck, click once inside the QEMU window to capture pointer focus.

### Expected behavior
- Graphical background + window + title bar + close button visual.
- Mouse cursor moves.
- Button shows pressed visual state and toggles background accent on click.
- Text field accepts basic printable keyboard input after focus click.

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

## Incremental Milestones (independently testable)
1. **Milestone 1**: Boot to blank colored screen.
2. **Milestone 2**: Render static text.
3. **Milestone 3**: Draw rectangle/window frame.
4. **Milestone 4**: Mouse cursor movement.
5. **Milestone 5**: Button click visual state.

## Testing Strategy
- **Framebuffer**: Draw deterministic patterns; verify coordinates, clipping, and colors.
- **Input events**: Log event transitions (mouse move/down/up, key down) to serial during debug runs.
- **Memory setup**: Validate GOP values (base/size/resolution/pitch) and confirm kernel loop remains stable.

## Future Extensions Roadmap
- Add optional PS/2 keyboard path.
- Add software backbuffer to reduce tearing.
- Add basic allocator + widget tree.
- Split into loader + freestanding kernel once MVP is stable.
- Later: interrupts/timer IRQs, filesystem, and process model.
