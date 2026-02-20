#ifndef INTERNAL_EVENT_CHANNEL_H
#define INTERNAL_EVENT_CHANNEL_H

#include "event_bus.h"

void event_channel_init(void);

BOOLEAN event_channel_enqueue(const event_packet_t *packet);
BOOLEAN event_channel_receive(UINT32 channel, event_packet_t *out_packet);
BOOLEAN event_channel_set_policy(UINT32 channel, event_backpressure_policy_t policy);
event_backpressure_policy_t event_channel_policy(UINT32 channel);
UINTN event_channel_depth(UINT32 channel);
UINT64 event_channel_drop_count(void);

BOOLEAN event_process_register(UINT32 pid);
void event_process_unregister(UINT32 pid);
BOOLEAN event_process_enqueue_targeted(const event_packet_t *packet);
BOOLEAN event_process_receive(UINT32 pid, event_packet_t *out_packet);
UINTN event_process_depth(UINT32 pid);
UINT64 event_process_drop_count(void);

#endif
