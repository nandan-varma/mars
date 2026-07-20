# MarsOS

A desktop operating system built from scratch — UEFI boot, a cooperative multitasking kernel, a real windowed GUI, and an event-driven application framework, all in freestanding C. No libc, no third-party OS libraries.

It runs in QEMU today. Real hardware is the goal.

## Boot sequence

The firmware loads `BOOTX64.EFI`. From there the boot entry locates the GOP framebuffer, keyboard, and pointer protocols via UEFI Boot Services, packs everything into a `boot_info_t` struct, and hands control to `kernel_main`. The kernel then brings up each subsystem in order: platform, diagnostics, interrupts, event bus, timer, scheduler, process manager, memory, virtual memory, heap, syscall interface, input drivers, window manager, VFS, and finally the application framework.

After that the scheduler loop runs forever. No OS call ever returns to firmware.

## The kernel

The scheduler is cooperative round-robin — tasks yield by returning from their step function. There's an optional timer-preemptive mode, but the main model is cooperative, which keeps the design simple and eliminates a whole class of concurrency bugs at this stage. Each process gets a capability bitmask (input, graphics, storage, system) that controls what it can access.

Memory is managed with a heap for small allocations and a page allocator for large ones. There's also a virtual memory subsystem and a simple FAT VFS for disk access.

## The GUI

The window manager renders directly to the UEFI GOP framebuffer in software — no GPU. It supports overlapping windows with z-ordering, a taskbar, a start menu, focus management, keyboard and mouse routing, and a resize handle on every window. Text is rendered with an 8x16 monospace bitmap font.

Apps don't touch the framebuffer directly. Instead each app maintains a UTF-16 text content buffer and calls `wm_set_window_content()`. The WM renders it. This keeps apps simple and the rendering logic centralized.

## The event bus

Everything in the OS communicates through an event bus. There are four broadcast channels (input, keyboard, system, app) plus per-process targeted queues. Events carry a 64-byte payload, a source PID, an optional target PID, and a backpressure policy (drop-oldest or drop-newest when a queue is full). The WM consumes raw input events, figures out which window has focus, and re-publishes them as targeted `APP_INPUT` events to the right process queue.

## Applications

Apps are kernel tasks with a manifest that declares an ID, title, initial window geometry, and capability flags. The task step function processes up to 12 events per cycle, updates the content buffer, and returns. Built-in apps:

- **Shell** — command interpreter
- **Terminal** — text terminal
- **File Browser** — VFS navigation
- **Settings** — system configuration
- **Task Manager** — live scheduler view
- **System Logs** — kernel log stream

## Drivers

The input stack supports UEFI keyboard and three mouse protocols (UEFI, PS/2, absolute pointer) with a priority ordering so it degrades gracefully. There's also a PCI driver, block device driver, audio driver skeleton, and a network driver stub.

## Testing

Because you can't unit test a kernel running inside QEMU easily, there's a host-test harness that compiles the same kernel source files against the host libc and runs contract tests on the event bus, scheduler, process model, memory allocator, heap, and VM builder. The suite runs in under a second — fast enough to run on every commit.

```bash
make -C os test-host   # host-side unit tests
make -C os run          # boot in QEMU
```

## Repository layout

```
os/
├── boot/       UEFI bootloader entry
├── kernel/     Scheduler, memory, interrupts, syscalls, event bus
├── drivers/    Input, PCI, block device, audio, network
├── graphics/   Framebuffer primitives, font rendering
├── gui/        Window manager, taskbar, start menu
├── apps/       Built-in applications
├── lib/        Freestanding libc-equivalent utilities
├── tests/      Host-side unit tests
└── scripts/    Build and QEMU run scripts
```

## Built with

- C (freestanding, `-ffreestanding -fno-stack-protector -mno-red-zone`) — everything
- UEFI / OVMF — firmware interface and boot
- QEMU — emulation target
- Docker — hermetic build environment matching CI byte-for-byte
- Make — build system with automatic toolchain detection (x86_64-elf-gcc, clang, MinGW)

## Key decisions

- **Cooperative over preemptive scheduling** — cooperative multitasking eliminates race conditions from involuntary preemption during initial development; tasks yield explicitly so shared state doesn't need locks
- **Bounded event queues with explicit backpressure** — unbounded queues hide producer/consumer mismatches until the system runs out of memory; fixed-capacity queues with a configurable drop policy make queue overflow a visible, debuggable event
- **Host-test harness over QEMU-only testing** — compiling the same source files against host libc lets the event bus, scheduler, and allocator have contract tests that run in under a second and catch regressions before boot
- **Freestanding C over C++ or Rust** — no standard library, no exceptions, no hidden constructors; every allocation is explicit
- **Direct framebuffer rendering over a retained-mode GUI** — every pixel is under direct control with no layout engine making decisions you can't inspect

## Future directions

- Preemptive scheduler with full context switching
- MMU and per-process memory isolation
- Persistent storage via the FAT VFS
- Networking stack
- Boot on real x86_64 hardware, not just QEMU
- App SDK so new apps can be added without modifying the kernel
