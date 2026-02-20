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

#ifndef SCHED_TIMER_PREEMPTIVE
#define SCHED_TIMER_PREEMPTIVE 0
#endif

static void on_timer_interrupt(UINTN vector, UINT64 a, UINT64 b, UINT64 c) {
    (void)vector;
    (void)a;
    (void)b;
    (void)c;
}

static void on_syscall_interrupt(UINTN vector, UINT64 a, UINT64 b, UINT64 c) {
    (void)vector;
    (void)b;
    (void)c;
    diag_log(0x200U, (UINT32)a, process_current_pid(), timer_ticks());
}

typedef struct {
    const CHAR16 *name;
    task_entry_t entry;
    UINT8 priority;
    UINT32 capabilities;
    UINT32 pid;
} managed_process_t;

static BOOLEAN input_task(void *context) {
    (void)context;
    input_poll();
    return TRUE;
}

static BOOLEAN wm_task(void *context) {
    (void)context;
    wm_dispatch_input();
    return TRUE;
}

static BOOLEAN render_task(void *context) {
    (void)context;
    if (wm_needs_redraw()) {
        wm_render();
    }

    return TRUE;
}

static managed_process_t g_managed_processes[] = {
    { L"input-service", input_task, 1, CAP_INPUT, 0 },
    { L"wm-service", wm_task, 1, CAP_GRAPHICS | CAP_INPUT, 0 },
    { L"render-service", render_task, 1, CAP_GRAPHICS, 0 }
};

static BOOLEAN spawn_managed_process(managed_process_t *managed) {
    if (managed == NULL || managed->entry == NULL) {
        return FALSE;
    }

    managed->pid = process_create_kernel(managed->name, managed->entry, NULL, managed->priority, managed->capabilities);
    if (managed->pid == 0) {
        diag_log(0x610U, 1, (UINT64)managed->priority, managed->capabilities);
        return FALSE;
    }

    diag_log(0x610U, 0, managed->pid, managed->capabilities);
    return TRUE;
}

static BOOLEAN supervisor_task(void *context) {
    (void)context;

    for (UINTN i = 0; i < (sizeof(g_managed_processes) / sizeof(g_managed_processes[0])); ++i) {
        managed_process_t *managed = &g_managed_processes[i];
        if (managed->pid != 0 && process_is_running(managed->pid)) {
            continue;
        }

        UINT32 previous_pid = managed->pid;
        if (spawn_managed_process(managed)) {
            diag_log(0x611U, 0, previous_pid, managed->pid);
        } else {
            diag_log(0x611U, 1, previous_pid, process_running_count());
        }
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
    diag_set_stage(10);
    interrupts_init();
    diag_set_stage(20);
    event_bus_init();
    (void)event_bus_set_channel_policy(EVENT_CHANNEL_INPUT, EVENT_BACKPRESSURE_DROP_OLDEST);
    (void)event_bus_set_channel_policy(EVENT_CHANNEL_INPUT_KEYBOARD, EVENT_BACKPRESSURE_DROP_OLDEST);
    (void)event_bus_set_channel_policy(EVENT_CHANNEL_SYSTEM, EVENT_BACKPRESSURE_DROP_NEWEST);
    (void)event_bus_set_channel_policy(EVENT_CHANNEL_APP, EVENT_BACKPRESSURE_DROP_NEWEST);
    diag_set_stage(30);
    timer_init(1000);
    diag_set_stage(40);
    scheduler_init();
    scheduler_set_timer_preemptive(SCHED_TIMER_PREEMPTIVE ? TRUE : FALSE);
    process_init();
    diag_set_stage(50);

    memory_init(platform);
    diag_set_stage(60);
    framebuffer_init(platform);
    diag_set_stage(70);
    vm_init(platform);
    diag_set_stage(80);
    heap_init(512);
    diag_set_stage(90);

    syscall_init();
    (void)interrupts_register(IRQ_VECTOR_TIMER, on_timer_interrupt);
    (void)interrupts_register(IRQ_VECTOR_SYSCALL, on_syscall_interrupt);

    input_init(platform, platform->framebuffer.width, platform->framebuffer.height);
    diag_set_stage(100);
    wm_init(platform->framebuffer.width, platform->framebuffer.height);
    diag_set_stage(110);

    vfs_init();
    vfs_block_device_t boot_device;
    boot_device.block_size = 512;
    boot_device.block_count = 16384;
    boot_device.read_only = TRUE;
    vfs_mount_boot_device(boot_device);
    diag_set_stage(120);

    for (UINTN i = 0; i < (sizeof(g_managed_processes) / sizeof(g_managed_processes[0])); ++i) {
        (void)spawn_managed_process(&g_managed_processes[i]);
    }

    UINT32 supervisor_pid = process_create_kernel(L"service-supervisor", supervisor_task, NULL, 1, CAP_SYSTEM);
    (void)supervisor_pid;

    app_framework_init();
    app_launch_core_suite();
    diag_set_stage(200);

    scheduler_run();
}
