#include "input.h"
#include "event_bus.h"
#include "keyboard_uefi.h"
#include "mouse_uefi.h"

static input_lifecycle_state_t g_state = INPUT_LIFECYCLE_UNINITIALIZED;
static UINT64 g_poll_count;
static UINT64 g_published_count;
static UINT64 g_drop_count;
static UINT64 g_keyboard_poll_count;
static UINT64 g_mouse_poll_count;

static BOOLEAN publish_input_event(const input_event_t *event) {
    if (event == NULL) {
        return FALSE;
    }

    event_packet_t packet;
    packet.channel = (event->type == INPUT_EVENT_KEY_DOWN)
        ? EVENT_CHANNEL_INPUT_KEYBOARD
        : EVENT_CHANNEL_INPUT;
    packet.code = EVENT_CODE_INPUT;
    packet.source_pid = 0;
    packet.target_pid = 0;
    packet.target_window = 0;
    packet.payload_size = sizeof(input_event_t);

    // PERFORMANCE FIX (Issue 5.3): Replace byte-by-byte loop with word-aligned copy
    // input_event_t is ~24 bytes; copy as 3 × 64-bit words
    const UINT64 *src_qwords = (const UINT64 *)event;
    UINT64 *payload_qwords = (UINT64 *)packet.payload;
    UINTN qword_count = (sizeof(input_event_t) + sizeof(UINT64) - 1) / sizeof(UINT64);
    for (UINTN i = 0; i < qword_count; ++i) {
        payload_qwords[i] = src_qwords[i];
    }
    // Zero-fill remaining payload
    for (UINTN i = qword_count * sizeof(UINT64); i < EVENT_PAYLOAD_BYTES; ++i) {
        packet.payload[i] = 0;
    }

    return event_bus_publish(&packet);
}

void input_init(const platform_context_t *platform, UINT32 screen_w, UINT32 screen_h) {
    g_poll_count = 0;
    g_published_count = 0;
    g_drop_count = 0;
    g_keyboard_poll_count = 0;
    g_mouse_poll_count = 0;

    if (platform == NULL) {
        g_state = INPUT_LIFECYCLE_UNINITIALIZED;
        return;
    }

    g_state = INPUT_LIFECYCLE_PROBED;

    keyboard_driver_init(platform->input.text_input_ex);
    mouse_driver_init(
        NULL,
        platform->input.simple_pointer,
        platform->input.absolute_pointer,
        screen_w,
        screen_h
    );

    g_state = INPUT_LIFECYCLE_STARTED;
}

void input_stop(void) {
    if (g_state == INPUT_LIFECYCLE_STARTED) {
        g_state = INPUT_LIFECYCLE_STOPPED;
    }
}

void input_poll(void) {
    if (g_state != INPUT_LIFECYCLE_STARTED) {
        return;
    }

    ++g_poll_count;

    input_event_t event;
    ++g_keyboard_poll_count;
    if (keyboard_driver_poll(&event)) {
        if (publish_input_event(&event)) {
            ++g_published_count;
        } else {
            ++g_drop_count;
        }
    }

    input_event_t mouse_events[3];
    ++g_mouse_poll_count;
    UINTN count = mouse_driver_poll(mouse_events, 3);
    for (UINTN i = 0; i < count; ++i) {
        if (publish_input_event(&mouse_events[i])) {
            ++g_published_count;
        } else {
            ++g_drop_count;
        }
    }
}

BOOLEAN input_pop_event(input_event_t *out_event) {
    if (out_event == NULL) {
        return FALSE;
    }

    event_packet_t packet;
    if (!event_bus_receive_channel(EVENT_CHANNEL_INPUT, &packet)) {
        if (!event_bus_receive_channel(EVENT_CHANNEL_INPUT_KEYBOARD, &packet)) {
            return FALSE;
        }
    }

    if (packet.code != EVENT_CODE_INPUT || packet.payload_size < sizeof(input_event_t)) {
        return FALSE;
    }

    // PERFORMANCE FIX (Issue 5.4): Replace byte-by-byte loop with word-aligned copy
    // input_event_t is ~24 bytes; extract as 3-4 × 64-bit words
    const UINT64 *payload_qwords = (const UINT64 *)packet.payload;
    UINT64 *event_qwords = (UINT64 *)out_event;
    UINTN qword_count = (sizeof(input_event_t) + sizeof(UINT64) - 1) / sizeof(UINT64);
    for (UINTN i = 0; i < qword_count; ++i) {
        event_qwords[i] = payload_qwords[i];
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

input_lifecycle_state_t input_lifecycle_state(void) {
    return g_state;
}

UINT64 input_poll_count(void) {
    return g_poll_count;
}

UINT64 input_published_count(void) {
    return g_published_count;
}

UINT64 input_drop_count(void) {
    return g_drop_count;
}

UINT64 input_keyboard_poll_count(void) {
    return g_keyboard_poll_count;
}

UINT64 input_mouse_poll_count(void) {
    return g_mouse_poll_count;
}
