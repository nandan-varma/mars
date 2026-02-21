#include "process.h"

#include "event_bus.h"
#include "spinlock.h"
#include "vm.h"

#define MAX_PROCESSES 64

static process_t g_processes[MAX_PROCESSES];
static UINT32 g_next_pid;
static UINT32 g_current_pid;
static spinlock_t g_process_lock;

static process_t *find_process(UINT32 pid) {
    for (UINTN i = 0; i < MAX_PROCESSES; ++i) {
        if (g_processes[i].pid == pid) {
            return &g_processes[i];
        }
    }
    return NULL;
}

static process_t *find_reusable_slot(void) {
    for (UINTN i = 0; i < MAX_PROCESSES; ++i) {
        if (g_processes[i].pid == 0 || g_processes[i].state == PROCESS_TERMINATED) {
            return &g_processes[i];
        }
    }
    return NULL;
}

void process_init(void) {
    spinlock_init(&g_process_lock);
    g_next_pid = 1;
    g_current_pid = 0;

    for (UINTN i = 0; i < MAX_PROCESSES; ++i) {
        g_processes[i].pid = 0;
        g_processes[i].state = PROCESS_NEW;
        g_processes[i].capabilities = 0;
        g_processes[i].exit_code = 0;
        g_processes[i].vm_root = 0;
        g_processes[i].task_id = 0;
    }
}

UINT32 process_create_kernel(const CHAR16 *name, task_entry_t entry, void *context, UINT8 priority, UINT32 capabilities, BOOLEAN is_kernel) {
    if (entry == NULL) {
        return 0;
    }

    if (!is_kernel && (capabilities & CAP_SYSTEM) != 0) {
        capabilities &= ~CAP_SYSTEM;
    }

    spinlock_acquire(&g_process_lock);

    process_t *process = find_reusable_slot();
    if (process == NULL) {
        spinlock_release(&g_process_lock);
        return 0;
    }

    process->pid = g_next_pid++;
    
    // SECURITY FIX #5: Prevent PID wraparound to 0 (which is invalid/kernel-reserved)
    // If next_pid wraps to 0, jump to 1 instead
    if (g_next_pid == 0) {
        g_next_pid = 1;
    }
    
    process->state = PROCESS_RUNNING;
    process->capabilities = capabilities;
    process->exit_code = 0;
    process->vm_root = vm_create_address_space();
    if (process->vm_root == 0) {
        process->vm_root = vm_pml4_physical();
    }

    spinlock_release(&g_process_lock);

    process->task_id = scheduler_create_task(process->pid, name, priority, entry, context);
    if (process->task_id == 0) {
        spinlock_acquire(&g_process_lock);
        if (process->vm_root != 0 && process->vm_root != vm_pml4_physical()) {
            (void)vm_release_address_space(process->vm_root);
        }
        process->pid = 0;
        process->state = PROCESS_NEW;
        process->exit_code = -1;
        spinlock_release(&g_process_lock);
        return 0;
    }

    (void)event_bus_register_process(process->pid);
    return process->pid;
}

void process_exit(UINT32 pid, INT32 exit_code) {
    spinlock_acquire(&g_process_lock);

    process_t *process = find_process(pid);
    if (process == NULL) {
        spinlock_release(&g_process_lock);
        return;
    }

    process->state = PROCESS_TERMINATED;
    process->exit_code = exit_code;
    UINT32 task_id = process->task_id;
    process->task_id = 0;
    EFI_PHYSICAL_ADDRESS vm_root = process->vm_root;
    process->vm_root = 0;

    // SECURITY FIX: Unregister from event bus BEFORE clearing pid
    // This prevents PID reuse while event queue still exists (CWE-362, CWE-416)
    event_bus_unregister_process(pid);

    // NOW clear pid after event bus is cleaned up
    process->pid = 0;

    spinlock_release(&g_process_lock);

    // All remaining operations happen with pid already cleared
    scheduler_stop_task(task_id);
    if (vm_root != 0 && vm_root != vm_pml4_physical()) {
        (void)vm_release_address_space(vm_root);
    }
}

BOOLEAN process_wait(UINT32 pid, INT32 *out_exit_code) {
    // SECURITY FIX (HIGH #5): Acquire lock to prevent use-after-free
    spinlock_acquire(&g_process_lock);
    process_t *process = find_process(pid);
    BOOLEAN result = FALSE;
    if (process != NULL && process->state == PROCESS_TERMINATED) {
        if (out_exit_code != NULL) {
            *out_exit_code = process->exit_code;
        }
        result = TRUE;
    }
    spinlock_release(&g_process_lock);
    return result;
}

UINT32 process_current_pid(void) {
    return g_current_pid;
}

void process_set_current_pid(UINT32 pid) {
    g_current_pid = pid;
}

UINT32 process_capabilities(UINT32 pid) {
    // SECURITY FIX (HIGH #5): Acquire lock to prevent use-after-free race
    // between find_process() and concurrent process_exit() clearing pid.
    // CWE-416: Use After Free
    spinlock_acquire(&g_process_lock);
    process_t *process = find_process(pid);
    UINT32 capabilities = 0;
    if (process != NULL) {
        capabilities = process->capabilities;
    }
    spinlock_release(&g_process_lock);
    return capabilities;
}

UINTN process_count(void) {
    UINTN count = 0;
    for (UINTN i = 0; i < MAX_PROCESSES; ++i) {
        if (g_processes[i].pid != 0) {
            ++count;
        }
    }
    return count;
}

UINTN process_running_count(void) {
    UINTN count = 0;
    for (UINTN i = 0; i < MAX_PROCESSES; ++i) {
        if (g_processes[i].state == PROCESS_RUNNING || g_processes[i].state == PROCESS_WAITING) {
            ++count;
        }
    }

    return count;
}

BOOLEAN process_is_running(UINT32 pid) {
    // SECURITY FIX (HIGH #5): Acquire lock to prevent use-after-free
    spinlock_acquire(&g_process_lock);
    process_t *process = find_process(pid);
    BOOLEAN running = FALSE;
    if (process != NULL) {
        running = process->state == PROCESS_RUNNING || process->state == PROCESS_WAITING;
    }
    spinlock_release(&g_process_lock);
    return running;
}
