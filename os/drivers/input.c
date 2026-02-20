#include "input.h"
#include "event_bus.h"
#include "keyboard_uefi.h"
#include "mouse_uefi.h"

static BOOLEAN publish_input_event(const input_event_t *event) {
    if (event == NULL) {
        return FALSE;
    }

    event_packet_t packet;
    packet.channel = EVENT_CHANNEL_INPUT;
    packet.code = EVENT_CODE_INPUT;
    packet.source_pid = 0;
    packet.target_pid = 0;
    packet.target_window = 0;
    packet.payload_size = sizeof(input_event_t);

    const UINT8 *src = (const UINT8 *)event;
    for (UINTN i = 0; i < sizeof(input_event_t); ++i) {
        packet.payload[i] = src[i];
    }
    for (UINTN i = sizeof(input_event_t); i < EVENT_PAYLOAD_BYTES; ++i) {
        packet.payload[i] = 0;
    }

    return event_bus_publish(&packet);
}

void input_init(const platform_context_t *platform, UINT32 screen_w, UINT32 screen_h) {
    if (platform == NULL) {
        return;
    }

    keyboard_driver_init(platform->input.text_input_ex);
    mouse_driver_init(
        NULL,
        platform->input.simple_pointer,
        platform->input.absolute_pointer,
        screen_w,
        screen_h
    );
}

void input_poll(void) {
    input_event_t event;
    if (keyboard_driver_poll(&event)) {
        (void)publish_input_event(&event);
    }

    input_event_t mouse_events[3];
    UINTN count = mouse_driver_poll(mouse_events, 3);
    for (UINTN i = 0; i < count; ++i) {
        (void)publish_input_event(&mouse_events[i]);
    }
}

BOOLEAN input_pop_event(input_event_t *out_event) {
    if (out_event == NULL) {
        return FALSE;
    }

    event_packet_t packet;
    if (!event_bus_receive_channel(EVENT_CHANNEL_INPUT, &packet)) {
        return FALSE;
    }

    if (packet.code != EVENT_CODE_INPUT || packet.payload_size < sizeof(input_event_t)) {
        return FALSE;
    }

    UINT8 *dst = (UINT8 *)out_event;
    for (UINTN i = 0; i < sizeof(input_event_t); ++i) {
        dst[i] = packet.payload[i];
    }

    return TRUE;
}

INT32 input_mouse_x(void) {
    return mouse_driver_x();
}

INT32 input_mouse_y(void) {
    return mouse_driver_y();
}

BOOLEAN input_left_down(void) {
    return mouse_driver_left_down();
}

BOOLEAN input_has_simple_mouse(void) {
    return mouse_driver_has_simple();
}

BOOLEAN input_has_absolute_mouse(void) {
    return mouse_driver_has_absolute();
}

BOOLEAN input_has_ps2_mouse(void) {
    return mouse_driver_has_ps2();
}
