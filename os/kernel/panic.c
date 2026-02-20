#include "panic.h"

#include "diag.h"

static panic_fault_t g_last_fault;

void panic_trap(UINTN vector, UINT64 code, UINT64 address) {
    g_last_fault.vector = vector;
    g_last_fault.code = code;
    g_last_fault.address = address;
    diag_capture_crash(vector, code, address);

    for (;;) {
    }
}

void panic_now(UINT64 code) {
    panic_trap(0xFFFFU, code, 0);
}

panic_fault_t panic_last_fault(void) {
    return g_last_fault;
}