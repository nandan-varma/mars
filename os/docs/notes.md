# Educational subsystem notes

## Boot and kernel model
- Chosen: a single UEFI PE app (`BOOTX64.EFI`) that directly runs `kernel_main`.
- Why: shortest path to reliable graphics + mouse + keyboard with low bring-up cost.
- Avoided: GRUB, second-stage ELF loading, ACPI setup, interrupt controller setup.

## Graphics
- Chosen: UEFI GOP linear framebuffer and CPU rasterization (`drawPixel`, `drawRect`, text blit).
- Why: deterministic, easy to debug, no GPU driver complexity.
- Avoided: hardware acceleration, compositor, transparency.

## Input
- Chosen: UEFI `SimpleTextInputEx` and `SimplePointer` protocols.
- Why: no USB/PS2 controller stack required in MVP.
- Avoided: USB HID stack and full PS/2 stack in early milestones.

## GUI
- Chosen: single-window immediate-mode style redraw per frame.
- Why: simplest event/render architecture for mouse + button + text field.
- Avoided: overlapping windows, z-order, desktop manager.

## Compare with Linux/Windows/MINIX
- Linux/Windows: complex driver models, scheduler, virtual memory, userspace, storage stack.
- MINIX: microkernel/message passing, still more complete process/file/driver model than this MVP.
- This OS: educational single-address-space prototype focused on boot + graphics + input fundamentals.
