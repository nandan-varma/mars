#ifndef PROCESS_H
#define PROCESS_H

#include "scheduler.h"

typedef enum {
    PROCESS_NEW = 0,
    PROCESS_RUNNING,
    PROCESS_WAITING,
    PROCESS_TERMINATED
} process_state_t;

typedef enum {
    CAP_INPUT = 1 << 0,
    CAP_GRAPHICS = 1 << 1,
    CAP_STORAGE = 1 << 2,
    CAP_SYSTEM = 1 << 3
} process_capability_t;

typedef struct {
    UINT32 pid;
    process_state_t state;
    UINT32 capabilities;
    INT32 exit_code;
    EFI_PHYSICAL_ADDRESS vm_root;
    UINT32 task_id;
} process_t;

void process_init(void);
UINT32 process_create_kernel(const CHAR16 *name, task_entry_t entry, void *context, UINT8 priority, UINT32 capabilities, BOOLEAN is_kernel);
void process_exit(UINT32 pid, INT32 exit_code);
BOOLEAN process_wait(UINT32 pid, INT32 *out_exit_code);
UINT32 process_current_pid(void);
void process_set_current_pid(UINT32 pid);
UINT32 process_capabilities(UINT32 pid);
UINTN process_count(void);
UINTN process_running_count(void);
BOOLEAN process_is_running(UINT32 pid);

#endif