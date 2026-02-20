#include "interrupts.h"

#include "diag.h"
#include "panic.h"

#define MAX_INTERRUPT_VECTORS 256

static interrupt_handler_t g_handlers[MAX_INTERRUPT_VECTORS];

void interrupts_init(void) {
    for (UINTN i = 0; i < MAX_INTERRUPT_VECTORS; ++i) {
        g_handlers[i] = NULL;
    }
}

BOOLEAN interrupts_register(UINTN vector, interrupt_handler_t handler) {
    if (vector >= MAX_INTERRUPT_VECTORS) {
        return FALSE;
    }

    g_handlers[vector] = handler;
    return TRUE;
}

void interrupts_dispatch(UINTN vector, UINT64 a, UINT64 b, UINT64 c) {
    if (vector >= MAX_INTERRUPT_VECTORS) {
        panic_trap(vector, a, b);
        return;
    }

    interrupt_handler_t handler = g_handlers[vector];
    if (handler != NULL) {
        handler(vector, a, b, c);
        return;
    }

    if (vector == IRQ_VECTOR_FAULT) {
        panic_trap(vector, a, b);
        return;
    }

    diag_log(0x100U, (UINT32)vector, a, b);
}