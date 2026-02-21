#include "mouse_internal.h"

// SECURITY FIX #3: PS/2 Buffer Overflow (CWE-680: Integer Overflow to Buffer Overflow)
// The original code incremented g_ps2_packet_index BEFORE checking bounds.
// This allowed index=3,4,5... to write past g_ps2_packet[2], corrupting g_mouse_x/y/g_left_down
// 
// VULNERABILITY: Line 23 writes g_ps2_packet[index++] then line 24 checks if index < 3
// When index becomes 3, it still writes g_ps2_packet[3] which is out of bounds
//
// FIX: Check bounds BEFORE writing, and reflow logic to process complete packets

UINTN poll_ps2_mouse(input_event_t *events_out, UINTN max_events) {
    if (!g_ps2_enabled || events_out == NULL || max_events == 0) {
        return 0;
    }

    UINTN event_count = 0;
    UINTN bytes_budget = 64;

    while (bytes_budget-- > 0 && (io_in8(0x64) & 0x01)) {
        UINT8 status = io_in8(0x64);
        UINT8 data = io_in8(0x60);

        if ((status & 0x20) == 0) {
            continue;
        }

        if (g_ps2_packet_index == 0 && (data & 0x08) == 0) {
            continue;
        }

        // SECURITY FIX (HIGH #13/Memory #13): Validate PS/2 packet flags
        // First byte (index 0) has reserved bits that should be 0 in certain configurations
        // CWE-20: Improper Input Validation
        if (g_ps2_packet_index == 0) {
            // Bit 6 and 7 should generally be 0 for valid PS/2 packets
            // Bit 3 must be 1 (already checked above)
            // Allow bits 0-2 (button flags) and 4-5 (overflow/sign bits)
            // Reject if reserved bits are set incorrectly
            if ((data & 0xC0) != 0) {
                // Reserved bits 6-7 are set - invalid packet, reset
                g_ps2_packet_index = 0;
                continue;
            }
        }

        // SECURITY: Check if packet is complete BEFORE writing more data
        if (g_ps2_packet_index >= 3) {
            // Packet is complete, process it first
            g_ps2_packet_index = 0;

            INT32 dx = (INT8)g_ps2_packet[1];
            INT32 dy = -(INT8)g_ps2_packet[2];

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
                if (event_count < max_events) {
                    events_out[event_count].type = INPUT_EVENT_MOUSE_MOVE;
                    events_out[event_count].data.mouse_move.dx = dx;
                    events_out[event_count].data.mouse_move.dy = dy;
                    events_out[event_count].data.mouse_move.x = g_mouse_x;
                    events_out[event_count].data.mouse_move.y = g_mouse_y;
                    ++event_count;
                }
            }

            BOOLEAN left_now = (g_ps2_packet[0] & 0x01) ? TRUE : FALSE;
            if (left_now != g_left_down && event_count < max_events) {
                events_out[event_count].type = left_now ? INPUT_EVENT_MOUSE_BUTTON_DOWN : INPUT_EVENT_MOUSE_BUTTON_UP;
                events_out[event_count].data.mouse_button.left = left_now;
                events_out[event_count].data.mouse_button.right = (g_ps2_packet[0] & 0x02) ? TRUE : FALSE;
                ++event_count;
                g_left_down = left_now;
            }

            if (event_count >= max_events) {
                break;
            }
        }

        // NOW safely write the new byte (index is guaranteed < 3)
        g_ps2_packet[g_ps2_packet_index++] = data;
    }

    return event_count;
}
