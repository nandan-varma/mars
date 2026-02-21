#include "scheduler.h"

#include "diag.h"
#include "interrupts.h"
#include "process.h"
#include "spinlock.h"
#include "timer.h"

#define MAX_TASKS 64

static task_t g_tasks[MAX_TASKS];
static UINTN g_task_count;
static UINT32 g_next_task_id;
static UINTN g_rr_index;
static BOOLEAN g_timer_preemptive;
static UINT64 g_last_task_tick;
static spinlock_t g_scheduler_lock;
// PERFORMANCE FIX: Bitmap for faster task state lookup
// Bit i set = task i is READY or RUNNING (not STOPPED)
// Allows O(1) bit check vs O(n) array scan
static UINT64 g_task_active_bitmap;

static void copy_name(CHAR16 *dst, const CHAR16 *src, UINTN max_chars) {
    if (max_chars == 0) {
        return;
    }

    if (src == NULL) {
        dst[0] = 0;
        return;
    }

    UINTN i = 0;
    while (i + 1 < max_chars && src[i] != 0) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = 0;
}

void scheduler_init(void) {
    spinlock_init(&g_scheduler_lock);
    g_task_count = 0;
    g_next_task_id = 1;
    g_rr_index = 0;
    g_timer_preemptive = FALSE;
    g_last_task_tick = 0;
    g_task_active_bitmap = 0;  // No tasks active initially
}

UINT32 scheduler_create_task(UINT32 owner_pid, const CHAR16 *name, UINT8 priority, task_entry_t entry, void *context) {
    if (entry == NULL) {
        return 0;
    }

    spinlock_acquire(&g_scheduler_lock);

    task_t *task = NULL;
    for (UINTN i = 0; i < g_task_count; ++i) {
        if (g_tasks[i].state == TASK_STOPPED) {
            task = &g_tasks[i];
            break;
        }
    }

    if (task == NULL) {
        if (g_task_count >= MAX_TASKS) {
            spinlock_release(&g_scheduler_lock);
            return 0;
        }
        task = &g_tasks[g_task_count++];
    }

    task->id = g_next_task_id++;
    task->owner_pid = owner_pid;
    task->priority = priority;
    task->state = TASK_READY;
    task->runtime_ticks = 0;
    task->entry = entry;
    task->context = context;
    copy_name(task->name, name, 24);
    
    // PERFORMANCE FIX: Update active bitmap
    // Find which slot this task occupies
    UINTN slot = task - &g_tasks[0];
    if (slot < MAX_TASKS) {
        g_task_active_bitmap |= (1ULL << slot);
    }

    spinlock_release(&g_scheduler_lock);
    return task->id;
}

void scheduler_stop_task(UINT32 task_id) {
    if (task_id == 0) {
        return;
    }

    spinlock_acquire(&g_scheduler_lock);

    for (UINTN i = 0; i < g_task_count; ++i) {
        if (g_tasks[i].id != task_id) {
            continue;
        }

        g_tasks[i].state = TASK_STOPPED;
        g_tasks[i].owner_pid = 0;
        g_tasks[i].entry = NULL;
        g_tasks[i].context = NULL;
        g_tasks[i].name[0] = 0;
        g_tasks[i].runtime_ticks = 0;
        
        // PERFORMANCE FIX: Update active bitmap
        if (i < MAX_TASKS) {
            g_task_active_bitmap &= ~(1ULL << i);
        }
        
        spinlock_release(&g_scheduler_lock);
        return;
    }

    spinlock_release(&g_scheduler_lock);
}

static void scheduler_dispatch_tick(void) {
    timer_poll();
    diag_set_tick(timer_ticks());
    interrupts_dispatch(IRQ_VECTOR_TIMER, timer_ticks(), timer_hz(), 0);
}

void scheduler_set_timer_preemptive(BOOLEAN enabled) {
    g_timer_preemptive = enabled;
}

BOOLEAN scheduler_timer_preemptive(void) {
    return g_timer_preemptive;
}

void scheduler_step(void) {
    if (g_task_count == 0) {
        scheduler_dispatch_tick();
        return;
    }

    scheduler_dispatch_tick();

    if (g_timer_preemptive) {
        UINT64 tick = timer_ticks();
        if (tick == g_last_task_tick) {
            return;
        }
        g_last_task_tick = tick;
    }

    // PERFORMANCE FIX #1: Check bitmap for fast empty-pool detection
    // If no active tasks (bitmap == 0), skip scheduling loop entirely
    if (g_task_active_bitmap == 0) {
        return;  // No active tasks, don't spin
    }

    UINTN start = g_rr_index;
    for (UINTN offset = 0; offset < g_task_count; ++offset) {
        UINTN index = (start + offset) % g_task_count;
        task_t *task = &g_tasks[index];
        
        // PERFORMANCE FIX #2: Use bitmap check to skip stopped tasks fast
        // If bit is not set, task is definitely stopped, skip without state check
        if (!(g_task_active_bitmap & (1ULL << index))) {
            continue;
        }
        
        if (task->owner_pid != 0 && !process_is_running(task->owner_pid)) {
            scheduler_stop_task(task->id);
            continue;
        }

        if ((task->state != TASK_READY && task->state != TASK_RUNNING) || task->entry == NULL) {
            continue;
        }

        task->state = TASK_RUNNING;
        process_set_current_pid(task->owner_pid);
        BOOLEAN keep_running = task->entry(task->context);
        process_set_current_pid(0);
        task->runtime_ticks += 1;
        task->state = keep_running ? TASK_READY : TASK_STOPPED;
        if (!keep_running && task->owner_pid != 0) {
            diag_log(0x501U, task->id, task->owner_pid, task->runtime_ticks);
            process_exit(task->owner_pid, -1);
        }
        g_rr_index = (index + 1) % g_task_count;
        break;
    }
}

void scheduler_run(void) {
    for (;;) {
        scheduler_step();
    }
}

UINTN scheduler_task_count(void) {
    UINTN active = 0;
    for (UINTN i = 0; i < g_task_count; ++i) {
        if (g_tasks[i].state != TASK_STOPPED && g_tasks[i].entry != NULL) {
            ++active;
        }
    }
    return active;
}

BOOLEAN scheduler_task_at(UINTN index, task_t *out_task) {
    if (out_task == NULL) {
        return FALSE;
    }

    UINTN active_index = 0;
    for (UINTN i = 0; i < g_task_count; ++i) {
        if (g_tasks[i].state == TASK_STOPPED || g_tasks[i].entry == NULL) {
            continue;
        }

        if (active_index == index) {
            *out_task = g_tasks[i];
            return TRUE;
        }

        ++active_index;
    }

    return FALSE;
}