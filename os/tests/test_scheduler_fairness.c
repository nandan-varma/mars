#include <assert.h>
#include <stdio.h>

#include "scheduler.h"

static UINT64 g_tick;
static int g_order[16];
static int g_order_count;
static int g_runs_a;
static int g_runs_b;
static int g_runs_c;

void timer_poll(void) { g_tick += 1; }
UINT64 timer_ticks(void) { return g_tick; }
UINTN timer_hz(void) { return 1000; }
void interrupts_dispatch(UINTN vector, UINT64 a, UINT64 b, UINT64 c) { (void)vector; (void)a; (void)b; (void)c; }
void diag_set_tick(UINT64 tick) { (void)tick; }
void diag_log(UINT32 domain, UINT32 code, UINT64 a, UINT64 b) { (void)domain; (void)code; (void)a; (void)b; }
BOOLEAN process_is_running(UINT32 pid) { return pid != 0; }
void process_set_current_pid(UINT32 pid) { (void)pid; }
void process_exit(UINT32 pid, INT32 exit_code) { (void)pid; (void)exit_code; }

static BOOLEAN task_a(void *ctx) {
    (void)ctx;
    g_order[g_order_count++] = 1;
    g_runs_a += 1;
    return TRUE;
}

static BOOLEAN task_b(void *ctx) {
    (void)ctx;
    g_order[g_order_count++] = 2;
    g_runs_b += 1;
    return TRUE;
}

static BOOLEAN task_c(void *ctx) {
    (void)ctx;
    g_order[g_order_count++] = 3;
    g_runs_c += 1;
    return TRUE;
}

int main(void) {
    static const CHAR16 name_a[] = { 'a', 0 };
    static const CHAR16 name_b[] = { 'b', 0 };
    static const CHAR16 name_c[] = { 'c', 0 };

    scheduler_init();
    (void)scheduler_create_task(1, name_a, 1, task_a, NULL);
    (void)scheduler_create_task(2, name_b, 1, task_b, NULL);
    (void)scheduler_create_task(3, name_c, 1, task_c, NULL);

    for (int i = 0; i < 6; ++i) {
        scheduler_step();
    }

    assert(g_runs_a == 2);
    assert(g_runs_b == 2);
    assert(g_runs_c == 2);
    assert(g_order_count == 6);
    assert(g_order[0] == 1 && g_order[1] == 2 && g_order[2] == 3);

    printf("scheduler fairness test passed\n");
    return 0;
}
