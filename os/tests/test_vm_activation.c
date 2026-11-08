#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define TEST_PASS(name) printf("[PASS] %s\n", name)
#define TEST_FAIL(name) printf("[FAIL] %s\n", name)

/* Host-side contract tests for vm_activate / process_vm_root call ordering.
 * These verify the logic without executing CR3 writes (which require ring 0). */

/* Simulate the vm_activate guard: pml4_phys == 0 → use kernel pml4. */
static uint64_t g_kernel_pml4 = 0xABCD000;

static uint64_t sim_vm_activate_target(uint64_t pml4_phys) {
    if (pml4_phys == 0) {
        pml4_phys = g_kernel_pml4;
    }
    return pml4_phys;
}

/* Test: kernel task (owner_pid == 0) falls back to kernel PML4. */
static int test_kernel_task_uses_kernel_pml4(void) {
    uint64_t result = sim_vm_activate_target(0);
    if (result == g_kernel_pml4) {
        TEST_PASS("kernel_task_uses_kernel_pml4");
        return 0;
    }
    TEST_FAIL("kernel_task_uses_kernel_pml4");
    return 1;
}

/* Test: user process with valid vm_root uses its own PML4. */
static int test_user_process_uses_own_pml4(void) {
    uint64_t process_pml4 = 0x1234000;
    uint64_t result = sim_vm_activate_target(process_pml4);
    if (result == process_pml4) {
        TEST_PASS("user_process_uses_own_pml4");
        return 0;
    }
    TEST_FAIL("user_process_uses_own_pml4");
    return 1;
}

/* Test: vm_activate(0) never passes 0 to CR3 (undefined behavior on real HW). */
static int test_zero_pml4_never_written(void) {
    uint64_t result = sim_vm_activate_target(0);
    if (result != 0) {
        TEST_PASS("zero_pml4_never_written");
        return 0;
    }
    TEST_FAIL("zero_pml4_never_written: would write 0 to CR3");
    return 1;
}

/* Test: after task completes, kernel PML4 is restored (not zero). */
static int test_kernel_pml4_restored_after_task(void) {
    uint64_t process_pml4 = 0x5678000;
    uint64_t before = sim_vm_activate_target(process_pml4);
    /* Simulate restore: vm_activate(vm_pml4_physical()) where pml4_physical() == g_kernel_pml4 */
    uint64_t after = sim_vm_activate_target(g_kernel_pml4);

    if (before == process_pml4 && after == g_kernel_pml4) {
        TEST_PASS("kernel_pml4_restored_after_task");
        return 0;
    }
    TEST_FAIL("kernel_pml4_restored_after_task");
    return 1;
}

/* Test: process with vm_root == 0 (fallback case in process_create_kernel) maps to kernel PML4. */
static int test_process_vm_root_zero_fallback(void) {
    /* When vm_create_address_space fails, process->vm_root = vm_pml4_physical() (non-zero).
     * If somehow vm_root is 0, vm_activate guards it. */
    uint64_t vm_root = 0;
    uint64_t result = sim_vm_activate_target(vm_root);
    if (result == g_kernel_pml4) {
        TEST_PASS("process_vm_root_zero_fallback");
        return 0;
    }
    TEST_FAIL("process_vm_root_zero_fallback");
    return 1;
}

int main(void) {
    int failures = 0;
    failures += test_kernel_task_uses_kernel_pml4();
    failures += test_user_process_uses_own_pml4();
    failures += test_zero_pml4_never_written();
    failures += test_kernel_pml4_restored_after_task();
    failures += test_process_vm_root_zero_fallback();

    if (failures == 0) {
        printf("All vm_activation tests passed.\n");
    } else {
        printf("%d vm_activation test(s) FAILED.\n", failures);
    }
    return failures;
}
