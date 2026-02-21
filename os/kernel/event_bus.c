#include "event_bus.h"

#include "internal/event_channel.h"

void event_bus_init(void) {
    event_channel_init();
}

BOOLEAN event_bus_register_process(UINT32 pid) {
    return event_process_register(pid);
}

void event_bus_unregister_process(UINT32 pid) {
    event_process_unregister(pid);
}

BOOLEAN event_bus_publish(const event_packet_t *packet) {
    if (packet == NULL) {
        return FALSE;
    }

    if (packet->payload_size > EVENT_PAYLOAD_BYTES) {
        return FALSE;
    }

    BOOLEAN accepted = FALSE;

    if (packet->channel < EVENT_CHANNEL_COUNT) {
        accepted = event_channel_enqueue(packet);
    }

    if (packet->target_pid == 0) {
        return accepted;
    }

    if (event_process_enqueue_targeted(packet)) {
        accepted = TRUE;
    }

    return accepted;
}

BOOLEAN event_bus_receive(UINT32 pid, event_packet_t *out_packet) {
    return event_process_receive(pid, out_packet);
}

BOOLEAN event_bus_receive_channel(UINT32 channel, event_packet_t *out_packet) {
    return event_channel_receive(channel, out_packet);
}

BOOLEAN event_bus_set_channel_policy(UINT32 channel, event_backpressure_policy_t policy) {
    return event_channel_set_policy(channel, policy);
}

event_backpressure_policy_t event_bus_channel_policy(UINT32 channel) {
    return event_channel_policy(channel);
}

UINTN event_bus_channel_depth(UINT32 channel) {
    return event_channel_depth(channel);
}

UINTN event_bus_process_depth(UINT32 pid) {
    return event_process_depth(pid);
}

UINT64 event_bus_channel_drop_count(void) {
    return event_channel_drop_count();
}

UINT64 event_bus_process_drop_count(void) {
    return event_process_drop_count();
}