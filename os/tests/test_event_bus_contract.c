#include <assert.h>
#include <stdio.h>

#include "event_bus.h"

static event_packet_t make_packet(UINT32 channel, UINT32 code) {
    event_packet_t packet;
    packet.channel = channel;
    packet.code = code;
    packet.source_pid = 1;
    packet.target_pid = 0;
    packet.target_window = 0;
    packet.payload_size = 0;
    for (UINTN i = 0; i < EVENT_PAYLOAD_BYTES; ++i) {
        packet.payload[i] = 0;
    }
    return packet;
}

int main(void) {
    event_bus_init();

    for (int i = 0; i < 160; ++i) {
        event_packet_t packet = make_packet(EVENT_CHANNEL_SYSTEM, EVENT_CODE_APP_LAUNCH_REQUEST);
        (void)event_bus_publish(&packet);
    }

    UINTN depth_newest = event_bus_channel_depth(EVENT_CHANNEL_SYSTEM);
    UINT64 drops_newest = event_bus_channel_drop_count();
    assert(depth_newest <= 127);
    assert(drops_newest > 0);

    (void)event_bus_set_channel_policy(EVENT_CHANNEL_SYSTEM, EVENT_BACKPRESSURE_DROP_OLDEST);

    UINT64 drops_before = event_bus_channel_drop_count();
    for (int i = 0; i < 80; ++i) {
        event_packet_t packet = make_packet(EVENT_CHANNEL_SYSTEM, EVENT_CODE_APP_HEARTBEAT);
        (void)event_bus_publish(&packet);
    }

    UINTN depth_oldest = event_bus_channel_depth(EVENT_CHANNEL_SYSTEM);
    UINT64 drops_after = event_bus_channel_drop_count();
    assert(depth_oldest <= 127);
    assert(drops_after >= drops_before);

    printf("event_bus contract test passed\n");
    return 0;
}
