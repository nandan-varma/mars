#include "internal/mouse_internal.h"

UINTN poll_absolute_pointer(input_event_t *events_out, UINTN max_events) {
    if (g_absolute_count == 0 || events_out == NULL || max_events == 0) {
        return 0;
    }

    for (UINTN protocol_index = 0; protocol_index < g_absolute_count; ++protocol_index) {
        EFI_ABSOLUTE_POINTER_PROTOCOL *protocol = g_absolute_protocols[protocol_index];
        EFI_ABSOLUTE_POINTER_STATE state;
        EFI_STATUS status = protocol->GetState(protocol, &state);
        if (status == EFI_NOT_READY || EFI_ERROR(status) || protocol->Mode == NULL) {
            continue;
        }

        UINT64 min_x = protocol->Mode->AbsoluteMinX;
        UINT64 max_x = protocol->Mode->AbsoluteMaxX;
        UINT64 min_y = protocol->Mode->AbsoluteMinY;
        UINT64 max_y = protocol->Mode->AbsoluteMaxY;

        INT32 mapped_x = g_mouse_x;
        INT32 mapped_y = g_mouse_y;

        // SECURITY FIX (HIGH #11): Add overflow checks before scaling mouse coordinates
        // Prevents integer overflow when mapping absolute coordinates to screen space.
        // CWE-190: Integer Overflow
        if (max_x > min_x && state.CurrentX >= min_x && state.CurrentX <= max_x) {
            UINT64 range = max_x - min_x;
            UINT64 current_offset = state.CurrentX - min_x;
            UINT64 screen_range = (UINT64)(g_screen_w - 1);
            
            // Check for potential overflow: current_offset * screen_range
            if (current_offset <= UINT64_MAX / screen_range) {
                mapped_x = (INT32)((current_offset * screen_range) / range);
            }
        }
        
        if (max_y > min_y && state.CurrentY >= min_y && state.CurrentY <= max_y) {
            UINT64 range = max_y - min_y;
            UINT64 current_offset = state.CurrentY - min_y;
            UINT64 screen_range = (UINT64)(g_screen_h - 1);
            
            // Check for potential overflow: current_offset * screen_range
            if (current_offset <= UINT64_MAX / screen_range) {
                mapped_y = (INT32)((current_offset * screen_range) / range);
            }
        }

        mapped_x = clamp(mapped_x, 0, g_screen_w - 1);
        mapped_y = clamp(mapped_y, 0, g_screen_h - 1);

        UINTN event_count = 0;
        INT32 dx = mapped_x - g_mouse_x;
        INT32 dy = mapped_y - g_mouse_y;

        if ((dx != 0 || dy != 0) && event_count < max_events) {
            g_mouse_x = mapped_x;
            g_mouse_y = mapped_y;
            events_out[event_count].type = INPUT_EVENT_MOUSE_MOVE;
            events_out[event_count].data.mouse_move.dx = dx;
            events_out[event_count].data.mouse_move.dy = dy;
            events_out[event_count].data.mouse_move.x = g_mouse_x;
            events_out[event_count].data.mouse_move.y = g_mouse_y;
            ++event_count;
        }

        BOOLEAN left_now = state.ActiveButtons != 0 ? TRUE : FALSE;
        if (left_now != g_left_down && event_count < max_events) {
            events_out[event_count].type = left_now ? INPUT_EVENT_MOUSE_BUTTON_DOWN : INPUT_EVENT_MOUSE_BUTTON_UP;
            events_out[event_count].data.mouse_button.left = left_now;
            events_out[event_count].data.mouse_button.right = FALSE;
            ++event_count;
            g_left_down = left_now;
        }

        if (event_count > 0) {
            return event_count;
        }
    }

    return 0;
}
