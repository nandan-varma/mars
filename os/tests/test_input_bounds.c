#include <assert.h>
#include <stdio.h>

#include "input.h"
#include "event_bus.h"

static event_packet_t g_packet;
static BOOLEAN g_has_packet;

void keyboard_driver_init(EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *protocol) { (void)protocol; }
BOOLEAN keyboard_driver_poll(input_event_t *event_out) { (void)event_out; return FALSE; }

void mouse_driver_init(EFI_BOOT_SERVICES *boot_services, EFI_SIMPLE_POINTER_PROTOCOL *simple_protocol, EFI_ABSOLUTE_POINTER_PROTOCOL *absolute_protocol, UINT32 screen_w, UINT32 screen_h) {
    (void)boot_services;
    (void)simple_protocol;
    (void)absolute_protocol;
    (void)screen_w;
    (void)screen_h;
}
UINTN mouse_driver_poll(input_event_t *events_out, UINTN max_events) { (void)events_out; (void)max_events; return 0; }
INT32 mouse_driver_x(void) { return 0; }
INT32 mouse_driver_y(void) { return 0; }
BOOLEAN mouse_driver_left_down(void) { return FALSE; }
BOOLEAN mouse_driver_has_simple(void) { return FALSE; }
BOOLEAN mouse_driver_has_absolute(void) { return FALSE; }
BOOLEAN mouse_driver_has_ps2(void) { return FALSE; }

BOOLEAN event_bus_publish(const event_packet_t *packet) { (void)packet; return TRUE; }
BOOLEAN event_bus_receive_channel(UINT32 channel, event_packet_t *out_packet) {
    (void)channel;
    if (!g_has_packet) {
        return FALSE;
    }

    *out_packet = g_packet;
    g_has_packet = FALSE;
    return TRUE;
}

static void set_packet(UINT32 code, UINT32 payload_size) {
    g_packet.channel = EVENT_CHANNEL_INPUT;
    g_packet.code = code;
    g_packet.source_pid = 0;
    g_packet.target_pid = 0;
    g_packet.target_window = 0;
    g_packet.payload_size = payload_size;
    for (UINTN i = 0; i < EVENT_PAYLOAD_BYTES; ++i) {
        g_packet.payload[i] = 0;
    }
    g_has_packet = TRUE;
}

int main(void) {
    input_event_t out;

    set_packet(999, sizeof(input_event_t));
    assert(!input_pop_event(&out));

    set_packet(EVENT_CODE_INPUT, sizeof(input_event_t) - 1);
    assert(!input_pop_event(&out));

    set_packet(EVENT_CODE_INPUT, sizeof(input_event_t));
    assert(input_pop_event(&out));

    printf("input bounds test passed\n");
    return 0;
}
