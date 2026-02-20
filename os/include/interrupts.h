#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include "uefi.h"

#define IRQ_VECTOR_TIMER 32
#define IRQ_VECTOR_FAULT 14
#define IRQ_VECTOR_SYSCALL 128

typedef void (*interrupt_handler_t)(UINTN vector, UINT64 a, UINT64 b, UINT64 c);

void interrupts_init(void);
BOOLEAN interrupts_register(UINTN vector, interrupt_handler_t handler);
void interrupts_dispatch(UINTN vector, UINT64 a, UINT64 b, UINT64 c);

#endif