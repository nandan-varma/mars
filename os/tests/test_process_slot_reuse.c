#include <assert.h>
#include <stdio.h>

#include "process.h"

static UINT32 g_next_task_id = 1;
static EFI_PHYSICAL_ADDRESS g_next_vm = 0x2000;

UINT32 scheduler_create_task(UINT32 owner_pid, const CHAR16 *name, UINT8 priority, task_entry_t entry, void *context) {
    (void)owner_pid;
    (void)name;
    (void)priority;
    (void)entry;
    (void)context;
    return g_next_task_id++;
}
void scheduler_stop_task(UINT32 task_id) { (void)task_id; }

BOOLEAN event_bus_register_process(UINT32 pid) { return pid != 0; }
void event_bus_unregister_process(UINT32 pid) { (void)pid; }

EFI_PHYSICAL_ADDRESS vm_pml4_physical(void) { return 0x1000; }
EFI_PHYSICAL_ADDRESS vm_create_address_space(void) { g_next_vm += 0x1000; return g_next_vm; }
BOOLEAN vm_release_address_space(EFI_PHYSICAL_ADDRESS root) { return root != 0; }

static BOOLEAN noop_task(void *ctx) {
    (void)ctx;
    return TRUE;
}

int main(void) {
    static const CHAR16 name_p[] = { 'p', 0 };
    static const CHAR16 name_overflow[] = { 'o','v','e','r','f','l','o','w', 0 };
    static const CHAR16 name_reused[] = { 'r','e','u','s','e','d', 0 };

    process_init();

    UINT32 pids[64];
    for (int i = 0; i < 64; ++i) {
        pids[i] = process_create_kernel(name_p, noop_task, NULL, 1, CAP_SYSTEM);
        assert(pids[i] != 0);
    }

    UINT32 overflow = process_create_kernel(name_overflow, noop_task, NULL, 1, CAP_SYSTEM);
    assert(overflow == 0);

    process_exit(pids[10], 0);
    UINT32 reused = process_create_kernel(name_reused, noop_task, NULL, 1, CAP_SYSTEM);
    assert(reused != 0);

    printf("process slot reuse test passed\n");
    return 0;
}
