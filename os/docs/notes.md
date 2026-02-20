# Educational subsystem notes

## Boot and kernel model
- Chosen: a single UEFI PE app (`BOOTX64.EFI`) that directly runs `kernel_main`.
- Runtime contract: Boot Services remain active (single supported mode).
- Why: shortest path to reliable graphics + input + service orchestration.
- Avoided: dual boot-mode maintenance and legacy GUI fallback paths.

## Graphics
- Chosen: UEFI GOP linear framebuffer and CPU rasterization (`drawPixel`, `drawRect`, text blit).
- WM path is canonical desktop renderer with a façade + split internals (`gui/wm.c`, `wm_state.c`, `wm_input.c`, `wm_render.c`, `wm_menu.c`).
- Avoided: hardware acceleration and full compositor pipeline.

## Input
- Chosen: UEFI `SimpleTextInputEx` plus layered mouse input (`AbsolutePointer`/`SimplePointer`) with bounded PS/2 fallback.
- Why: keeps MVP mostly UEFI-native while providing a deterministic fallback path on firmware/device combinations where UEFI pointer protocols are unreliable.
- Avoided: USB HID stack and full multitier input subsystem complexity.

## Windowing and apps
- Chosen: multi-window WM with taskbar/start menu and app-managed content.
- Ownership split: WM handles focus/input/composition, app manager handles launch/lifecycle.
- App launch path: start menu publishes `EVENT_CODE_APP_LAUNCH_REQUEST` on system channel.
- App internals are split for clarity: command dispatcher (`app_console_cmd.c`), manifest registry (`app_registry.c`), instance lifecycle (`app_instance.c`).
- Avoided: direct WM → app-launch coupling.

## Scheduling and lifecycle
- Chosen: cooperative scheduler with priority-aware selection and reusable task slots.
- Process lifecycle now includes explicit task stop on exit and reusable process slots.
- Avoided: preemptive context switching for now.

## Eventing
- Event bus remains bounded and lock-free with fixed queues.
- Added telemetry for dropped channel/process events to improve observability.
- Public API remains in `event_bus.c` with internals split into `event_packet.c` and `event_channel.c`.
- Avoided: unbounded queues or blocking producers.

## Internal seams and invariants
- Mouse polling is strategy-split: `mouse_ps2.c`, `mouse_absolute.c`, `mouse_simple.c`; orchestration priority stays PS/2 → Absolute → Simple.
- Memory allocator is partitioned: page-region internals (`memory_pages.c`) and free-list internals (`memory_freelist.c`) behind `memory.c` API.
- VM table construction is isolated in `vm_builder.c` while `vm.c` owns address-space lifecycle.
- Kernel bring-up is table-driven in `kernel.c`; stage markers and init ordering are unchanged.

## Compare with Linux/Windows/MINIX
- Linux/Windows: complex driver models, scheduler, virtual memory, userspace, storage stack.
- MINIX: microkernel/message passing, still more complete process/file/driver model than this MVP.
- This OS: educational single-address-space desktop prototype focused on boot + evented services + WM/app architecture.
