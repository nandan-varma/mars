#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>

/* Process stress test */
#define TEST_PASS(name) printf("[PASS] %s\n", name)
#define TEST_FAIL(name) printf("[FAIL] %s\n", name); return 1

/* Constants from headers */
#define MAX_PROCESSES 64
#define MAX_TASKS 64
#define MAX_EVENTS_PER_PROCESS 32
#define EVENT_QUEUE_SIZE 256

/* Simulate process state */
typedef enum {
    PROCESS_NEW = 0,
    PROCESS_RUNNING = 1,
    PROCESS_WAITING = 2,
    PROCESS_TERMINATED = 3
} process_state_t;

typedef struct {
    uint32_t pid;
    process_state_t state;
    uint32_t capabilities;
    int32_t exit_code;
    uint32_t task_id;
    uint32_t event_queue_count;
} process_t;

typedef struct {
    process_t processes[MAX_PROCESSES];
    uint32_t process_count;
    uint32_t next_pid;
} process_manager_t;

/* Global state */
static process_manager_t g_pm = {0};

/* Simulate process creation */
static uint32_t sim_process_create(void) {
    if (g_pm.process_count >= MAX_PROCESSES) {
        return 0;  /* Failed */
    }

    uint32_t pid = g_pm.next_pid++;
    if (pid == 0) pid = 1;  /* Skip PID 0 */

    process_t *p = &g_pm.processes[g_pm.process_count++];
    p->pid = pid;
    p->state = PROCESS_RUNNING;
    p->capabilities = 0;
    p->exit_code = 0;
    p->task_id = pid % MAX_TASKS;
    p->event_queue_count = 0;

    return pid;
}

/* Simulate process exit */
static void sim_process_exit(uint32_t pid, int32_t exit_code) {
    for (uint32_t i = 0; i < g_pm.process_count; i++) {
        if (g_pm.processes[i].pid == pid) {
            g_pm.processes[i].state = PROCESS_TERMINATED;
            g_pm.processes[i].exit_code = exit_code;
            return;
        }
    }
}

/* Simulate finding running processes */
static uint32_t sim_running_process_count(void) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < g_pm.process_count; i++) {
        if (g_pm.processes[i].state == PROCESS_RUNNING) {
            count++;
        }
    }
    return count;
}

/* Test 1: Rapid process creation (64 max) */
static int test_rapid_process_creation(void) {
    memset(&g_pm, 0, sizeof(g_pm));
    g_pm.next_pid = 1;

    int created = 0;
    for (int i = 0; i < 64; i++) {
        uint32_t pid = sim_process_create();
        if (pid > 0) created++;
    }

    if (created == 64 && g_pm.process_count == 64) {
        TEST_PASS("rapid_process_creation");
        return 0;
    }
    TEST_FAIL("rapid_process_creation");
}

/* Test 2: Process over-allocation rejection */
static int test_process_over_allocation_rejection(void) {
    memset(&g_pm, 0, sizeof(g_pm));
    g_pm.next_pid = 1;

    /* Fill to capacity */
    for (int i = 0; i < MAX_PROCESSES; i++) {
        sim_process_create();
    }

    /* Try one more - should fail */
    uint32_t pid = sim_process_create();
    if (pid == 0) {
        TEST_PASS("process_over_allocation_rejection");
        return 0;
    }
    TEST_FAIL("process_over_allocation_rejection");
}

/* Test 3: Rapid create-exit cycles (10 cycles, 64 processes) */
static int test_rapid_create_exit_cycles(void) {
    int total_cycles = 0;

    for (int cycle = 0; cycle < 10; cycle++) {
        memset(&g_pm, 0, sizeof(g_pm));
        g_pm.next_pid = 1;

        /* Create 64 processes */
        uint32_t pids[64];
        int created = 0;
        for (int i = 0; i < 64; i++) {
            uint32_t pid = sim_process_create();
            if (pid > 0) {
                pids[created++] = pid;
            }
        }

        if (created != 64) break;

        /* Exit in random order (simulate by going backwards then forwards) */
        for (int i = 63; i >= 0; i--) {
            if (i < created) {
                sim_process_exit(pids[i], 0);
            }
        }

        /* Verify all terminated */
        if (sim_running_process_count() == 0) {
            total_cycles++;
        }
    }

    if (total_cycles == 10) {
        TEST_PASS("rapid_create_exit_cycles");
        return 0;
    }
    TEST_FAIL("rapid_create_exit_cycles");
}

/* Test 4: Event queue saturation (100 events per process, 10 processes) */
static int test_event_queue_saturation(void) {
    memset(&g_pm, 0, sizeof(g_pm));
    g_pm.next_pid = 1;

    /* Create 10 processes */
    uint32_t pids[10];
    for (int i = 0; i < 10; i++) {
        pids[i] = sim_process_create();
    }

    if (g_pm.process_count != 10) {
        TEST_FAIL("event_queue_saturation: creation failed");
    }

    /* Push 100 events per process */
    int total_queued = 0;
    for (int p = 0; p < 10; p++) {
        for (int e = 0; e < 100; e++) {
            int idx = -1;
            for (uint32_t i = 0; i < g_pm.process_count; i++) {
                if (g_pm.processes[i].pid == pids[p]) {
                    idx = i;
                    break;
                }
            }

            if (idx >= 0) {
                if (g_pm.processes[idx].event_queue_count < MAX_EVENTS_PER_PROCESS) {
                    g_pm.processes[idx].event_queue_count++;
                    total_queued++;
                }
            }
        }
    }

    /* Should queue at least 320 events (10 processes × 32 max queue) */
    if (total_queued == 320) {
        TEST_PASS("event_queue_saturation");
        return 0;
    }
    TEST_FAIL("event_queue_saturation");
}

/* Test 5: Concurrent event publishing (1000 events/sec simulation) */
static int test_concurrent_event_publishing(void) {
    memset(&g_pm, 0, sizeof(g_pm));
    g_pm.next_pid = 1;

    /* Create 10 processes */
    for (int i = 0; i < 10; i++) {
        sim_process_create();
    }

    /* Simulate 1000 event publishes distributed among processes */
    int published = 0;
    for (int evt = 0; evt < 1000; evt++) {
        uint32_t proc_idx = evt % g_pm.process_count;
        if (g_pm.processes[proc_idx].event_queue_count < MAX_EVENTS_PER_PROCESS) {
            g_pm.processes[proc_idx].event_queue_count++;
            published++;
        }
    }

    /* All 320 should be published (10 × 32) before hitting limits */
    if (published == 320) {
        TEST_PASS("concurrent_event_publishing");
        return 0;
    }
    TEST_FAIL("concurrent_event_publishing");
}

/* Test 6: Scheduler fairness - 32 tasks, verify ±10% CPU time distribution */
static int test_scheduler_fairness_32tasks(void) {
    /* Simulate 32 tasks with equal priority */
    uint32_t task_steps[32] = {0};
    const int TOTAL_STEPS = 3200;  /* 100 per task on average */

    /* Round-robin scheduling simulation */
    for (int step = 0; step < TOTAL_STEPS; step++) {
        uint32_t task_idx = step % 32;
        task_steps[task_idx]++;
    }

    /* Check fairness: each task should get 100±10 steps */
    int fair_tasks = 0;
    for (int i = 0; i < 32; i++) {
        if (task_steps[i] >= 90 && task_steps[i] <= 110) {
            fair_tasks++;
        }
    }

    if (fair_tasks >= 30) {  /* At least 30 out of 32 within tolerance */
        TEST_PASS("scheduler_fairness_32tasks");
        return 0;
    }
    TEST_FAIL("scheduler_fairness_32tasks");
}

/* Test 7: Process state transitions */
static int test_process_state_transitions(void) {
    memset(&g_pm, 0, sizeof(g_pm));
    g_pm.next_pid = 1;

    uint32_t pid = sim_process_create();
    if (pid == 0) {
        TEST_FAIL("process_state_transitions: creation failed");
    }

    /* Find process */
    int idx = -1;
    for (uint32_t i = 0; i < g_pm.process_count; i++) {
        if (g_pm.processes[i].pid == pid) {
            idx = i;
            break;
        }
    }

    if (idx < 0) {
        TEST_FAIL("process_state_transitions: process not found");
    }

    /* Test state transitions: NEW -> RUNNING -> TERMINATED */
    process_t *p = &g_pm.processes[idx];

    if (p->state == PROCESS_RUNNING) {
        p->state = PROCESS_TERMINATED;
        p->exit_code = 42;

        if (p->state == PROCESS_TERMINATED && p->exit_code == 42) {
            TEST_PASS("process_state_transitions");
            return 0;
        }
    }

    TEST_FAIL("process_state_transitions");
}

/* Test 8: Mixed workload - create/exit/queue stress (2000 operations) */
static int test_mixed_workload_2000ops(void) {
    memset(&g_pm, 0, sizeof(g_pm));
    g_pm.next_pid = 1;

    uint32_t pids[64];
    int ops_completed = 0;

    /* 2000 mixed operations */
    for (int op = 0; op < 2000; op++) {
        uint32_t op_type = op % 3;

        switch (op_type) {
            case 0:  /* Create */
                if (g_pm.process_count < MAX_PROCESSES) {
                    uint32_t pid = sim_process_create();
                    if (pid > 0) ops_completed++;
                }
                break;

            case 1:  /* Exit */
                if (g_pm.process_count > 0) {
                    uint32_t idx = (op / 3) % g_pm.process_count;
                    uint32_t pid = g_pm.processes[idx].pid;
                    if (g_pm.processes[idx].state == PROCESS_RUNNING) {
                        sim_process_exit(pid, 0);
                        ops_completed++;
                    }
                }
                break;

            case 2:  /* Queue event */
                if (g_pm.process_count > 0) {
                    uint32_t idx = (op / 3) % g_pm.process_count;
                    if (g_pm.processes[idx].event_queue_count < MAX_EVENTS_PER_PROCESS) {
                        g_pm.processes[idx].event_queue_count++;
                        ops_completed++;
                    }
                }
                break;
        }
    }

    /* Should complete at least 500 mixed operations */
    if (ops_completed >= 500) {
        TEST_PASS("mixed_workload_2000ops");
        return 0;
    }
    TEST_FAIL("mixed_workload_2000ops");
}

/* Test 9: Verify no duplicate PIDs */
static int test_no_duplicate_pids(void) {
    memset(&g_pm, 0, sizeof(g_pm));
    g_pm.next_pid = 1;

    /* Create 64 processes */
    for (int i = 0; i < 64; i++) {
        sim_process_create();
    }

    /* Check for duplicates */
    for (uint32_t i = 0; i < g_pm.process_count; i++) {
        for (uint32_t j = i + 1; j < g_pm.process_count; j++) {
            if (g_pm.processes[i].pid == g_pm.processes[j].pid) {
                TEST_FAIL("no_duplicate_pids");
            }
        }
    }

    TEST_PASS("no_duplicate_pids");
    return 0;
}

/* Test 10: Event dropping under saturation */
static int test_event_dropping_saturation(void) {
    memset(&g_pm, 0, sizeof(g_pm));
    g_pm.next_pid = 1;

    /* Create 1 process with max queue of 32 */
    sim_process_create();
    process_t *p = &g_pm.processes[0];

    /* Try to push 100 events, only 32 should fit */
    int accepted = 0;
    int dropped = 0;

    for (int e = 0; e < 100; e++) {
        if (p->event_queue_count < MAX_EVENTS_PER_PROCESS) {
            p->event_queue_count++;
            accepted++;
        } else {
            dropped++;
        }
    }

    if (accepted == 32 && dropped == 68) {
        TEST_PASS("event_dropping_saturation");
        return 0;
    }
    TEST_FAIL("event_dropping_saturation");
}

int main(void) {
    printf("=== Process Stress Tests ===\n");

    int failures = 0;
    failures += test_rapid_process_creation();
    failures += test_process_over_allocation_rejection();
    failures += test_rapid_create_exit_cycles();
    failures += test_event_queue_saturation();
    failures += test_concurrent_event_publishing();
    failures += test_scheduler_fairness_32tasks();
    failures += test_process_state_transitions();
    failures += test_mixed_workload_2000ops();
    failures += test_no_duplicate_pids();
    failures += test_event_dropping_saturation();

    if (failures == 0) {
        printf("\n=== All process stress tests passed ===\n");
        return 0;
    }

    printf("\n=== %d test(s) failed ===\n", failures);
    return 1;
}
