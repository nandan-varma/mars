#include "internal/event_channel.h"

#include "event_packet.h"

#define EVENT_QUEUE_CAPACITY 128
#define EVENT_MAX_PROCESSES 64

typedef struct {
    event_packet_t queue[EVENT_QUEUE_CAPACITY];
    UINTN head;
    UINTN tail;
    BOOLEAN active;
    UINT32 pid;
} process_event_queue_t;

typedef struct {
    event_packet_t queue[EVENT_QUEUE_CAPACITY];
    UINTN head;
    UINTN tail;
} channel_event_queue_t;

static process_event_queue_t g_queues[EVENT_MAX_PROCESSES];
static channel_event_queue_t g_channel_queues[EVENT_CHANNEL_COUNT];
static event_backpressure_policy_t g_channel_policy[EVENT_CHANNEL_COUNT];
static UINT64 g_channel_drop_count;
static UINT64 g_process_drop_count;

static BOOLEAN queue_push(event_packet_t *queue, UINTN *head, UINTN *tail, const event_packet_t *packet) {
    UINTN next = (*tail + 1) % EVENT_QUEUE_CAPACITY;
    if (next == *head) {
        return FALSE;
    }

    event_packet_copy(&queue[*tail], packet);
    *tail = next;
    return TRUE;
}

static BOOLEAN queue_pop(event_packet_t *queue, UINTN *head, UINTN *tail, event_packet_t *out_packet) {
    if (*head == *tail) {
        return FALSE;
    }

    event_packet_copy(out_packet, &queue[*head]);
    *head = (*head + 1) % EVENT_QUEUE_CAPACITY;
    return TRUE;
}

static UINTN queue_depth(UINTN head, UINTN tail) {
    if (tail >= head) {
        return tail - head;
    }

    return EVENT_QUEUE_CAPACITY - (head - tail);
}

void event_channel_init(void) {
    g_channel_drop_count = 0;
    g_process_drop_count = 0;

    for (UINTN channel = 0; channel < EVENT_CHANNEL_COUNT; ++channel) {
        g_channel_queues[channel].head = 0;
        g_channel_queues[channel].tail = 0;
        g_channel_policy[channel] = EVENT_BACKPRESSURE_DROP_NEWEST;
    }

    for (UINTN i = 0; i < EVENT_MAX_PROCESSES; ++i) {
        g_queues[i].head = 0;
        g_queues[i].tail = 0;
        g_queues[i].active = FALSE;
        g_queues[i].pid = 0;
    }
}

BOOLEAN event_channel_enqueue(const event_packet_t *packet) {
    if (packet == NULL || packet->channel >= EVENT_CHANNEL_COUNT) {
        return FALSE;
    }

    channel_event_queue_t *channel_queue = &g_channel_queues[packet->channel];
    BOOLEAN accepted = queue_push(channel_queue->queue, &channel_queue->head, &channel_queue->tail, packet);
    if (!accepted) {
        if (g_channel_policy[packet->channel] == EVENT_BACKPRESSURE_DROP_OLDEST) {
            event_packet_t dropped;
            if (queue_pop(channel_queue->queue, &channel_queue->head, &channel_queue->tail, &dropped)) {
                accepted = queue_push(channel_queue->queue, &channel_queue->head, &channel_queue->tail, packet);
            }
        }

        if (!accepted) {
            ++g_channel_drop_count;
        }
    }

    return accepted;
}

BOOLEAN event_channel_receive(UINT32 channel, event_packet_t *out_packet) {
    if (out_packet == NULL || channel >= EVENT_CHANNEL_COUNT) {
        return FALSE;
    }

    channel_event_queue_t *channel_queue = &g_channel_queues[channel];
    return queue_pop(channel_queue->queue, &channel_queue->head, &channel_queue->tail, out_packet);
}

BOOLEAN event_channel_set_policy(UINT32 channel, event_backpressure_policy_t policy) {
    if (channel >= EVENT_CHANNEL_COUNT) {
        return FALSE;
    }

    g_channel_policy[channel] = policy;
    return TRUE;
}

event_backpressure_policy_t event_channel_policy(UINT32 channel) {
    if (channel >= EVENT_CHANNEL_COUNT) {
        return EVENT_BACKPRESSURE_DROP_NEWEST;
    }

    return g_channel_policy[channel];
}

UINTN event_channel_depth(UINT32 channel) {
    if (channel >= EVENT_CHANNEL_COUNT) {
        return 0;
    }

    channel_event_queue_t *queue = &g_channel_queues[channel];
    return queue_depth(queue->head, queue->tail);
}

UINT64 event_channel_drop_count(void) {
    return g_channel_drop_count;
}

BOOLEAN event_process_register(UINT32 pid) {
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

void event_process_unregister(UINT32 pid) {
    for (UINTN i = 0; i < EVENT_MAX_PROCESSES; ++i) {
        if (g_queues[i].active && g_queues[i].pid == pid) {
            g_queues[i].active = FALSE;
            g_queues[i].head = 0;
            g_queues[i].tail = 0;
            g_queues[i].pid = 0;
        }
    }
}

BOOLEAN event_process_enqueue_targeted(const event_packet_t *packet) {
    if (packet == NULL || packet->target_pid == 0) {
        return FALSE;
    }

    BOOLEAN accepted = FALSE;
    for (UINTN i = 0; i < EVENT_MAX_PROCESSES; ++i) {
        if (!g_queues[i].active) {
            continue;
        }

        if (packet->target_pid != g_queues[i].pid) {
            continue;
        }

        if (queue_push(g_queues[i].queue, &g_queues[i].head, &g_queues[i].tail, packet)) {
            accepted = TRUE;
        } else {
            ++g_process_drop_count;
        }
    }

    return accepted;
}

BOOLEAN event_process_receive(UINT32 pid, event_packet_t *out_packet) {
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

UINTN event_process_depth(UINT32 pid) {
    for (UINTN i = 0; i < EVENT_MAX_PROCESSES; ++i) {
        if (!g_queues[i].active || g_queues[i].pid != pid) {
            continue;
        }

        return queue_depth(g_queues[i].head, g_queues[i].tail);
    }

    return 0;
}

UINT64 event_process_drop_count(void) {
    return g_process_drop_count;
}
