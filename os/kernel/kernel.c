#include "kernel.h"
#include "app.h"
#include "diag.h"
#include "event_bus.h"
#include "framebuffer.h"
#include "heap.h"
#include "input.h"
#include "interrupts.h"
#include "memory.h"
#include "platform.h"
#include "process.h"
#include "scheduler.h"
#include "syscall.h"
#include "timer.h"
#include "vfs.h"
#include "vm.h"
#include "wm.h"

static void on_timer_interrupt(UINTN vector, UINT64 a, UINT64 b, UINT64 c) {
    (void)vector;
    (void)b;
    (void)c;

    event_packet_t packet;
    packet.channel = EVENT_CHANNEL_SYSTEM;
    packet.code = EVENT_CODE_TIMER_TICK;
    packet.source_pid = 0;
    packet.target_pid = 0;
    packet.target_window = 0;
    packet.payload_size = sizeof(UINT64);
    UINT8 *bytes = (UINT8 *)&a;
    for (UINTN i = 0; i < sizeof(UINT64); ++i) {
        packet.payload[i] = bytes[i];
    }
    for (UINTN i = sizeof(UINT64); i < EVENT_PAYLOAD_BYTES; ++i) {
        packet.payload[i] = 0;
    }
    (void)event_bus_publish(&packet);
}

static void on_syscall_interrupt(UINTN vector, UINT64 a, UINT64 b, UINT64 c) {
    (void)vector;
    (void)b;
    (void)c;
    diag_log(0x200U, (UINT32)a, process_current_pid(), timer_ticks());
}

static BOOLEAN desktop_task(void *context) {
    (void)context;

    input_poll();
    wm_dispatch_input();

    if (wm_needs_redraw()) {
        wm_render();
    }

    return TRUE;
}

void kernel_main(const boot_info_t *boot_info) {
    if (boot_info == NULL) {
        return;
    }

    platform_init_from_boot(boot_info);
    const platform_context_t *platform = platform_context();
    if (platform == NULL) {
        return;
    }

    diag_init();
    interrupts_init();
    event_bus_init();
    timer_init(1000);
    scheduler_init();
    process_init();

    memory_init(platform);
    framebuffer_init(platform);
    vm_init(platform);
    heap_init(512);

    syscall_init();
    (void)interrupts_register(IRQ_VECTOR_TIMER, on_timer_interrupt);
    (void)interrupts_register(IRQ_VECTOR_SYSCALL, on_syscall_interrupt);

    input_init(platform, platform->framebuffer.width, platform->framebuffer.height);
    wm_init(platform->framebuffer.width, platform->framebuffer.height);

    vfs_init();
    vfs_block_device_t boot_device;
    boot_device.block_size = 512;
    boot_device.block_count = 16384;
    boot_device.read_only = TRUE;
    vfs_mount_boot_device(boot_device);

    UINT32 desktop_pid = process_create_kernel(L"desktop-shell", desktop_task, NULL, 1, CAP_SYSTEM | CAP_INPUT | CAP_GRAPHICS);
    (void)desktop_pid;

    app_framework_init();
    app_launch_core_suite();

    scheduler_run();
}
