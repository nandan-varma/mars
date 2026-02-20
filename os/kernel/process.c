#include "process.h"

#include "event_bus.h"
#include "vm.h"

#define MAX_PROCESSES 64

static process_t g_processes[MAX_PROCESSES];
static UINTN g_process_count;
static UINT32 g_next_pid;
static UINT32 g_current_pid;

static process_t *find_process(UINT32 pid) {
    for (UINTN i = 0; i < g_process_count; ++i) {
        if (g_processes[i].pid == pid) {
            return &g_processes[i];
        }
    }
    return NULL;
}

void process_init(void) {
    g_process_count = 0;
    g_next_pid = 1;
    g_current_pid = 0;
}

UINT32 process_create_kernel(const CHAR16 *name, task_entry_t entry, void *context, UINT8 priority, UINT32 capabilities) {
    if (g_process_count >= MAX_PROCESSES || entry == NULL) {
        return 0;
    }

    process_t *process = &g_processes[g_process_count++];
    process->pid = g_next_pid++;
    process->state = PROCESS_RUNNING;
    process->capabilities = capabilities;
    process->exit_code = 0;
    process->vm_root = vm_pml4_physical();
    process->task_id = scheduler_create_task(process->pid, name, priority, entry, context);
    if (process->task_id == 0) {
        process->state = PROCESS_TERMINATED;
        process->exit_code = -1;
        return 0;
    }

    (void)event_bus_register_process(process->pid);
    return process->pid;
}

void process_exit(UINT32 pid, INT32 exit_code) {
    process_t *process = find_process(pid);
    if (process == NULL) {
        return;
    }

    process->state = PROCESS_TERMINATED;
    process->exit_code = exit_code;
    event_bus_unregister_process(pid);
}

BOOLEAN process_wait(UINT32 pid, INT32 *out_exit_code) {
    process_t *process = find_process(pid);
    if (process == NULL || process->state != PROCESS_TERMINATED) {
        return FALSE;
    }

    if (out_exit_code != NULL) {
        *out_exit_code = process->exit_code;
    }
    return TRUE;
}

UINT32 process_current_pid(void) {
    return g_current_pid;
}

void process_set_current_pid(UINT32 pid) {
    g_current_pid = pid;
}

UINT32 process_capabilities(UINT32 pid) {
    process_t *process = find_process(pid);
    if (process == NULL) {
        return 0;
    }
    return process->capabilities;
}

UINTN process_count(void) {
    return g_process_count;
}

BOOLEAN process_is_running(UINT32 pid) {
    process_t *process = find_process(pid);
    if (process == NULL) {
        return FALSE;
    }

    return process->state == PROCESS_RUNNING || process->state == PROCESS_WAITING;
}