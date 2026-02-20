#include "event_bus.h"

#define EVENT_QUEUE_CAPACITY 128
#define EVENT_MAX_PROCESSES 64

typedef struct {
    event_packet_t queue[EVENT_QUEUE_CAPACITY];
    UINTN head;
    UINTN tail;
    BOOLEAN active;
    UINT32 pid;
} process_event_queue_t;

static process_event_queue_t g_queues[EVENT_MAX_PROCESSES];
static event_packet_t g_channel_queue[EVENT_QUEUE_CAPACITY];
static UINTN g_channel_head;
static UINTN g_channel_tail;

static void copy_packet(event_packet_t *dst, const event_packet_t *src) {
    const UINT8 *src_bytes = (const UINT8 *)src;
    UINT8 *dst_bytes = (UINT8 *)dst;
    for (UINTN i = 0; i < sizeof(event_packet_t); ++i) {
        dst_bytes[i] = src_bytes[i];
    }
}

static BOOLEAN queue_push(event_packet_t *queue, UINTN *head, UINTN *tail, const event_packet_t *packet) {
    UINTN next = (*tail + 1) % EVENT_QUEUE_CAPACITY;
    if (next == *head) {
        return FALSE;
    }

    copy_packet(&queue[*tail], packet);
    *tail = next;
    return TRUE;
}

static BOOLEAN queue_pop(event_packet_t *queue, UINTN *head, UINTN *tail, event_packet_t *out_packet) {
    if (*head == *tail) {
        return FALSE;
    }

    copy_packet(out_packet, &queue[*head]);
    *head = (*head + 1) % EVENT_QUEUE_CAPACITY;
    return TRUE;
}

void event_bus_init(void) {
    g_channel_head = 0;
    g_channel_tail = 0;

    for (UINTN i = 0; i < EVENT_MAX_PROCESSES; ++i) {
        g_queues[i].head = 0;
        g_queues[i].tail = 0;
        g_queues[i].active = FALSE;
        g_queues[i].pid = 0;
    }
}

BOOLEAN event_bus_register_process(UINT32 pid) {
    for (UINTN i = 0; i < EVENT_MAX_PROCESSES; ++i) {
        if (g_queues[i].active && g_queues[i].pid == pid) {
            return TRUE;
        }
    }

    for (UINTN i = 0; i < EVENT_MAX_PROCESSES; ++i) {
        if (!g_queues[i].active) {
            g_queues[i].active = TRUE;
            g_queues[i].pid = pid;
            g_queues[i].head = 0;
            g_queues[i].tail = 0;
            return TRUE;
        }
    }

    return FALSE;
}

void event_bus_unregister_process(UINT32 pid) {
    for (UINTN i = 0; i < EVENT_MAX_PROCESSES; ++i) {
        if (g_queues[i].active && g_queues[i].pid == pid) {
            g_queues[i].active = FALSE;
            g_queues[i].head = 0;
            g_queues[i].tail = 0;
            g_queues[i].pid = 0;
        }
    }
}

BOOLEAN event_bus_publish(const event_packet_t *packet) {
    if (packet == NULL) {
        return FALSE;
    }

    BOOLEAN accepted = queue_push(g_channel_queue, &g_channel_head, &g_channel_tail, packet);

    for (UINTN i = 0; i < EVENT_MAX_PROCESSES; ++i) {
        if (!g_queues[i].active) {
            continue;
        }

        if (packet->target_pid != 0 && packet->target_pid != g_queues[i].pid) {
            continue;
        }

        if (queue_push(g_queues[i].queue, &g_queues[i].head, &g_queues[i].tail, packet)) {
            accepted = TRUE;
        }
    }

    return accepted;
}

BOOLEAN event_bus_receive(UINT32 pid, event_packet_t *out_packet) {
    if (out_packet == NULL) {
        return FALSE;
    }

    for (UINTN i = 0; i < EVENT_MAX_PROCESSES; ++i) {
        if (!g_queues[i].active || g_queues[i].pid != pid) {
            continue;
        }
        return queue_pop(g_queues[i].queue, &g_queues[i].head, &g_queues[i].tail, out_packet);
    }

    return FALSE;
}

BOOLEAN event_bus_receive_channel(UINT32 channel, event_packet_t *out_packet) {
    if (out_packet == NULL) {
        return FALSE;
    }

    event_packet_t packet;
    UINTN start = g_channel_head;
    while (queue_pop(g_channel_queue, &g_channel_head, &g_channel_tail, &packet)) {
        if (packet.channel == channel) {
            copy_packet(out_packet, &packet);
            return TRUE;
        }

        if (!queue_push(g_channel_queue, &g_channel_head, &g_channel_tail, &packet)) {
            return FALSE;
        }

        if (g_channel_head == start) {
            break;
        }
    }

    return FALSE;
}