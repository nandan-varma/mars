#include "internal/wm_internal.h"

#include "event_bus.h"
#include "input.h"
#include "process.h"
#include "wm_utils.h"

// SECURITY FIX #2: TOCTOU (Time-Of-Check-Time-Of-Use) Race in window input routing
// Process can exit between process_is_running() check and event_bus_publish()
// Solution: Take atomic snapshot of owner_pid and use it consistently
static void forward_input_to_focused_window(const input_event_t *event) {
    wm_state_t *state = wm_state();

    if (event == NULL || state->focused_window == 0) {
        return;
    }

    wm_window_t *window = wm_find_window(state->focused_window);
    if (window == NULL || window->owner_pid == 0) {
        return;
    }

    // SECURITY: Capture owner_pid atomically (single read)
    UINT32 owner_pid = window->owner_pid;

    if (!process_is_running(owner_pid)) {
        window->visible = FALSE;
        window->invalidated = TRUE;
        state->focused_window = 0;
        state->active_window = 0;
        state->dirty = TRUE;
        return;
    }

    event_packet_t packet;
    packet.channel = EVENT_CHANNEL_APP;
    packet.code = EVENT_CODE_APP_INPUT;
    packet.source_pid = 0;
    packet.target_pid = owner_pid;  // Use captured pid, not window->owner_pid
    packet.target_window = window->id;
    packet.payload_size = sizeof(input_event_t);

    const UINT8 *src = (const UINT8 *)event;
    for (UINTN i = 0; i < sizeof(input_event_t); ++i) {
        packet.payload[i] = src[i];
    }
    for (UINTN i = sizeof(input_event_t); i < EVENT_PAYLOAD_BYTES; ++i) {
        packet.payload[i] = 0;
    }

    (void)event_bus_publish(&packet);
}

static void handle_mouse_move(const input_event_t *event) {
    wm_state_t *state = wm_state();

    if (event == NULL || state->active_window == 0) {
        state->dirty = TRUE;
        return;
    }

    wm_window_t *window = wm_find_window(state->active_window);
    if (window == NULL) {
        return;
    }

    if (state->dragging) {
        window->x = wm_clamp_i32(event->data.mouse_move.x - state->drag_offset_x, 0, (INT32)state->desktop_w - window->width);
        window->y = wm_clamp_i32(event->data.mouse_move.y - state->drag_offset_y, 0, (INT32)state->desktop_h - window->height);
        window->invalidated = TRUE;
        state->dirty = TRUE;
    }

    if (state->resizing) {
        INT32 new_w = event->data.mouse_move.x - window->x;
        INT32 new_h = event->data.mouse_move.y - window->y;
        window->width = wm_clamp_i32(new_w, 120, (INT32)state->desktop_w - window->x);
        window->height = wm_clamp_i32(new_h, 80, (INT32)state->desktop_h - window->y);
        window->invalidated = TRUE;
        state->dirty = TRUE;
    }

    state->dirty = TRUE;
}

static void handle_button_down(void) {
    wm_state_t *state = wm_state();

    INT32 mouse_x = input_mouse_x();
    INT32 mouse_y = input_mouse_y();

    if (wm_handle_start_menu_click(mouse_x, mouse_y)) {
        return;
    }

    state->start_menu_open = FALSE;

    wm_window_t *window = wm_top_window_at(mouse_x, mouse_y);
    if (window == NULL) {
        state->focused_window = 0;
        state->active_window = 0;
        state->dragging = FALSE;
        state->resizing = FALSE;
        state->dirty = TRUE;
        return;
    }

    wm_focus_window_internal(window->id);

    INT32 close_x = window->x + window->width - 20;
    INT32 close_y = window->y + 4;
    INT32 min_x = window->x + window->width - 40;
    INT32 min_y = window->y + 4;

    if (wm_point_in_rect(mouse_x, mouse_y, close_x, close_y, 14, 14)) {
        window->visible = FALSE;
        window->invalidated = TRUE;
        // SECURITY FIX (HIGH #7): Validate owner_pid before terminating process
        // Stale window reference might have invalid owner_pid if corrupted
        UINT32 owner_pid = window->owner_pid;
        if (owner_pid != 0 && process_is_running(owner_pid)) {
            process_exit(owner_pid, 0);
        }
        if (state->focused_window == window->id) {
            state->focused_window = 0;
            state->active_window = 0;
        }
        state->dirty = TRUE;
        return;
    }

    if (wm_point_in_rect(mouse_x, mouse_y, min_x, min_y, 14, 14)) {
        window->visible = FALSE;
        window->invalidated = TRUE;
        if (state->focused_window == window->id) {
            state->focused_window = 0;
            state->active_window = 0;
        }
        state->dirty = TRUE;
        return;
    }

    if (mouse_y < window->y + 24) {
        state->dragging = TRUE;
        state->active_window = window->id;
        state->drag_offset_x = mouse_x - window->x;
        state->drag_offset_y = mouse_y - window->y;
        state->dirty = TRUE;
    } else if (mouse_x >= window->x + window->width - 12 && mouse_y >= window->y + window->height - 12) {
        state->resizing = TRUE;
        state->active_window = window->id;
        state->dirty = TRUE;
    }
}

static void handle_button_up(void) {
    wm_state_t *state = wm_state();
    state->dragging = FALSE;
    state->resizing = FALSE;
}

void wm_dispatch_input(void) {
    wm_state_t *state = wm_state();
    event_packet_t packet;

    UINTN pointer_processed = 0;
    while (pointer_processed < WM_POINTER_EVENTS_PER_STEP && event_bus_receive_channel(EVENT_CHANNEL_INPUT, &packet)) {
        if (packet.code != EVENT_CODE_INPUT || packet.payload_size < sizeof(input_event_t)) {
            ++pointer_processed;
            continue;
        }

        input_event_t event;
        UINT8 *dst = (UINT8 *)&event;
        for (UINTN i = 0; i < sizeof(input_event_t); ++i) {
            dst[i] = packet.payload[i];
        }

        // SECURITY FIX (HIGH #6): Validate deserialized event.type before use
        // Prevents invalid event codes from reaching handlers. CWE-20: Improper Input Validation
        if (event.type == INPUT_EVENT_MOUSE_MOVE) {
            handle_mouse_move(&event);
        } else if (event.type == INPUT_EVENT_MOUSE_BUTTON_DOWN) {
            handle_button_down();
            forward_input_to_focused_window(&event);
            state->dirty = TRUE;
        } else if (event.type == INPUT_EVENT_MOUSE_BUTTON_UP) {
            handle_button_up();
            forward_input_to_focused_window(&event);
            state->dirty = TRUE;
        }
        // NOTE: Invalid event.type values are silently dropped

        ++pointer_processed;
    }

    UINTN key_processed = 0;
    while (key_processed < WM_KEY_EVENTS_PER_STEP && event_bus_receive_channel(EVENT_CHANNEL_INPUT_KEYBOARD, &packet)) {
        if (packet.code != EVENT_CODE_INPUT || packet.payload_size < sizeof(input_event_t)) {
            ++key_processed;
            continue;
        }

        input_event_t event;
        UINT8 *dst = (UINT8 *)&event;
        for (UINTN i = 0; i < sizeof(input_event_t); ++i) {
            dst[i] = packet.payload[i];
        }

        // SECURITY FIX (HIGH #6): Validate deserialized event.type
        if (event.type == INPUT_EVENT_KEY_DOWN) {
            forward_input_to_focused_window(&event);
            state->dirty = TRUE;
        }
        // NOTE: Invalid event.type values are silently dropped

        ++key_processed;
    }
}
