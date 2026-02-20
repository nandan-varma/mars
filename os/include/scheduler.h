#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "uefi.h"

typedef enum {
    TASK_READY = 0,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_STOPPED
} task_state_t;

typedef BOOLEAN (*task_entry_t)(void *context);

typedef struct {
    UINT32 id;
    UINT32 owner_pid;
    UINT8 priority;
    task_state_t state;
    UINT64 runtime_ticks;
    CHAR16 name[24];
    task_entry_t entry;
    void *context;
} task_t;

void scheduler_init(void);
UINT32 scheduler_create_task(UINT32 owner_pid, const CHAR16 *name, UINT8 priority, task_entry_t entry, void *context);
void scheduler_stop_task(UINT32 task_id);
void scheduler_run(void);
void scheduler_step(void);
UINTN scheduler_task_count(void);
BOOLEAN scheduler_task_at(UINTN index, task_t *out_task);

#endif