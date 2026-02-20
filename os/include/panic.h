#ifndef PANIC_H
#define PANIC_H

#include "uefi.h"

typedef struct {
    UINTN vector;
    UINT64 code;
    UINT64 address;
} panic_fault_t;

void panic_trap(UINTN vector, UINT64 code, UINT64 address);
void panic_now(UINT64 code);
panic_fault_t panic_last_fault(void);

#endif