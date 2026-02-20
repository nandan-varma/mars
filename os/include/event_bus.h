#ifndef EVENT_BUS_H
#define EVENT_BUS_H

#include "uefi.h"

#define EVENT_CHANNEL_INPUT 1
#define EVENT_CHANNEL_SYSTEM 2
#define EVENT_CHANNEL_APP 3

#define EVENT_CODE_INPUT 1
#define EVENT_CODE_TIMER_TICK 2
#define EVENT_CODE_APP_HEARTBEAT 3
#define EVENT_CODE_APP_INPUT 4

#define EVENT_PAYLOAD_BYTES 64

typedef struct {
    UINT32 channel;
    UINT32 code;
    UINT32 source_pid;
    UINT32 target_pid;
    UINT32 target_window;
    UINT32 payload_size;
    UINT8 payload[EVENT_PAYLOAD_BYTES];
} event_packet_t;

void event_bus_init(void);
BOOLEAN event_bus_register_process(UINT32 pid);
void event_bus_unregister_process(UINT32 pid);
BOOLEAN event_bus_publish(const event_packet_t *packet);
BOOLEAN event_bus_receive(UINT32 pid, event_packet_t *out_packet);
BOOLEAN event_bus_receive_channel(UINT32 channel, event_packet_t *out_packet);

#endif