#include "syscall.h"

#include "diag.h"
#include "event_bus.h"
#include "interrupts.h"
#include "process.h"
#include "timer.h"

static syscall_handler_t g_syscalls[SYS_MAX];

static UINT64 syscall_nop(UINT64 a, UINT64 b, UINT64 c, UINT64 d) {
    (void)a;
    (void)b;
    (void)c;
    (void)d;
    return 0;
}

static UINT64 syscall_log_handler(UINT64 domain, UINT64 code, UINT64 a, UINT64 b) {
    diag_log((UINT32)domain, (UINT32)code, a, b);
    return 0;
}

static UINT64 syscall_send_event_handler(UINT64 packet_ptr, UINT64 b, UINT64 c, UINT64 d) {
    (void)b;
    (void)c;
    (void)d;

    const event_packet_t *packet = (const event_packet_t *)(UINTN)packet_ptr;
    if (packet == NULL) {
        return 1;
    }

    if ((packet_ptr & (sizeof(UINTN) - 1)) != 0) {
        return 4;
    }

    if (packet->channel >= EVENT_CHANNEL_COUNT || packet->payload_size > EVENT_PAYLOAD_BYTES) {
        return 5;
    }

    UINT32 pid = process_current_pid();
    if (pid == 0) {
        return 6;
    }

    if (packet->source_pid != 0 && packet->source_pid != pid) {
        return 7;
    }

    UINT32 caps = process_capabilities(pid);
    if ((caps & CAP_SYSTEM) == 0 && packet->channel == EVENT_CHANNEL_SYSTEM) {
        return 2;
    }

    event_packet_t sanitized = *packet;
    sanitized.source_pid = pid;
    return event_bus_publish(&sanitized) ? 0 : 3;
}

static UINT64 syscall_get_ticks_handler(UINT64 a, UINT64 b, UINT64 c, UINT64 d) {
    (void)a;
    (void)b;
    (void)c;
    (void)d;
    return timer_ticks();
}

void syscall_init(void) {
    for (UINTN i = 0; i < SYS_MAX; ++i) {
        g_syscalls[i] = syscall_nop;
    }

    g_syscalls[SYS_LOG] = syscall_log_handler;
    g_syscalls[SYS_SEND_EVENT] = syscall_send_event_handler;
    g_syscalls[SYS_GET_TICKS] = syscall_get_ticks_handler;
}

BOOLEAN syscall_register(UINTN id, syscall_handler_t handler) {
    if (id >= SYS_MAX || handler == NULL) {
        return FALSE;
    }

    g_syscalls[id] = handler;
    return TRUE;
}

UINT64 syscall_dispatch(UINTN id, UINT64 a, UINT64 b, UINT64 c, UINT64 d) {
    if (id >= SYS_MAX) {
        return (UINT64)-1;
    }
    return g_syscalls[id](a, b, c, d);
}

UINT64 syscall_enter(UINTN id, UINT64 a, UINT64 b, UINT64 c, UINT64 d) {
    interrupts_dispatch(IRQ_VECTOR_SYSCALL, id, a, b);
    return syscall_dispatch(id, a, b, c, d);
}