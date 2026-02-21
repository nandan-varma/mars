#ifndef EVENT_BUS_H
#define EVENT_BUS_H

#include "uefi.h"

#define EVENT_CHANNEL_INPUT 1
#define EVENT_CHANNEL_INPUT_KEYBOARD 2
#define EVENT_CHANNEL_SYSTEM 3
#define EVENT_CHANNEL_APP 4
#define EVENT_CHANNEL_COUNT 5

#define EVENT_CODE_INPUT 1
#define EVENT_CODE_TIMER_TICK 2
#define EVENT_CODE_APP_HEARTBEAT 3
#define EVENT_CODE_APP_INPUT 4
#define EVENT_CODE_APP_LAUNCH_REQUEST 5

#define EVENT_PAYLOAD_BYTES 64

typedef enum {
    EVENT_BACKPRESSURE_DROP_NEWEST = 0,
    EVENT_BACKPRESSURE_DROP_OLDEST = 1
} event_backpressure_policy_t;

typedef struct {
    UINT32 channel;
    UINT32 code;
    UINT32 source_pid;
    UINT32 target_pid;
    UINT32 target_window;
    UINT32 payload_size;
    UINT8 payload[EVENT_PAYLOAD_BYTES];
} event_packet_t;

// COMPILE-TIME SAFETY: Ensure input_event_t fits in payload
#include "input.h"
_Static_assert(sizeof(input_event_t) <= EVENT_PAYLOAD_BYTES, 
    "input_event_t must fit in event_packet_t payload");

void event_bus_init(void);
BOOLEAN event_bus_register_process(UINT32 pid);
void event_bus_unregister_process(UINT32 pid);
BOOLEAN event_bus_publish(const event_packet_t *packet);
BOOLEAN event_bus_receive(UINT32 pid, event_packet_t *out_packet);
BOOLEAN event_bus_receive_channel(UINT32 channel, event_packet_t *out_packet);
BOOLEAN event_bus_set_channel_policy(UINT32 channel, event_backpressure_policy_t policy);
event_backpressure_policy_t event_bus_channel_policy(UINT32 channel);
UINTN event_bus_channel_depth(UINT32 channel);
UINTN event_bus_process_depth(UINT32 pid);
UINT64 event_bus_channel_drop_count(void);
UINT64 event_bus_process_drop_count(void);

#endif