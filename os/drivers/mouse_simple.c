#include "internal/mouse_internal.h"

UINTN poll_simple_pointer(input_event_t *events_out, UINTN max_events) {
    if (g_simple_count == 0 || events_out == NULL || max_events == 0) {
        return 0;
    }

    for (UINTN protocol_index = 0; protocol_index < g_simple_count; ++protocol_index) {
        EFI_SIMPLE_POINTER_PROTOCOL *protocol = g_simple_protocols[protocol_index];
        EFI_SIMPLE_POINTER_STATE state;
        EFI_STATUS status = protocol->GetState(protocol, &state);
        if (status == EFI_NOT_READY || EFI_ERROR(status)) {
            continue;
        }

        UINTN event_count = 0;

        INT32 dx = state.RelativeMovementX;
        INT32 dy = state.RelativeMovementY;

        if (dx > 16 || dx < -16) {
            dx /= 4;
        }
        if (dy > 16 || dy < -16) {
            dy /= 4;
        }

        if (dx != 0 || dy != 0) {
            INT32 new_x = g_mouse_x + dx;
            INT32 new_y = g_mouse_y + dy;
            if (dx > 0 && new_x < g_mouse_x) {
                new_x = INT32_MAX;
            } else if (dx < 0 && new_x > g_mouse_x) {
                new_x = INT32_MIN;
            }
            if (dy > 0 && new_y < g_mouse_y) {
                new_y = INT32_MAX;
            } else if (dy < 0 && new_y > g_mouse_y) {
                new_y = INT32_MIN;
            }
            g_mouse_x = clamp(new_x, 0, g_screen_w - 1);
            g_mouse_y = clamp(new_y, 0, g_screen_h - 1);

            events_out[event_count].type = INPUT_EVENT_MOUSE_MOVE;
            events_out[event_count].data.mouse_move.dx = dx;
            events_out[event_count].data.mouse_move.dy = dy;
            events_out[event_count].data.mouse_move.x = g_mouse_x;
            events_out[event_count].data.mouse_move.y = g_mouse_y;
            ++event_count;
        }

        BOOLEAN left_now = state.LeftButton ? TRUE : FALSE;
        if (left_now != g_left_down && event_count < max_events) {
            events_out[event_count].type = left_now ? INPUT_EVENT_MOUSE_BUTTON_DOWN : INPUT_EVENT_MOUSE_BUTTON_UP;
            events_out[event_count].data.mouse_button.left = left_now;
            events_out[event_count].data.mouse_button.right = state.RightButton ? TRUE : FALSE;
            ++event_count;
            g_left_down = left_now;
        }

        if (event_count > 0) {
            return event_count;
        }
    }

    return 0;
}
