#ifndef SYSCALL_H
#define SYSCALL_H

#include "uefi.h"

typedef enum {
    SYS_NOP = 0,
    SYS_LOG = 1,
    SYS_SEND_EVENT = 2,
    SYS_GET_TICKS = 3,
    SYS_MAX = 64
} syscall_id_t;

typedef UINT64 (*syscall_handler_t)(UINT64 a, UINT64 b, UINT64 c, UINT64 d);

void syscall_init(void);
BOOLEAN syscall_register(UINTN id, syscall_handler_t handler);
UINT64 syscall_dispatch(UINTN id, UINT64 a, UINT64 b, UINT64 c, UINT64 d);
UINT64 syscall_enter(UINTN id, UINT64 a, UINT64 b, UINT64 c, UINT64 d);

#endif