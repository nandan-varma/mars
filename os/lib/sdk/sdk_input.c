#include "sdk/sdk_input.h"

// Static storage for last known mouse state
static sdk_mouse_state_t g_last_mouse_state = {0, 0, FALSE, FALSE, FALSE};

// ============================================================================
// Input Event Type Detection
// ============================================================================

BOOLEAN sdk_input_is_key_down(const input_event_t *event) {
    if (event == NULL) {
        return FALSE;
    }
    return event->type == INPUT_EVENT_KEY_DOWN;
}

BOOLEAN sdk_input_is_key_up(const input_event_t *event) {
    if (event == NULL) {
        return FALSE;
    }
    // Kernel doesn't generate KEY_UP events, only KEY_DOWN
    // Return FALSE for now
    return FALSE;
}

BOOLEAN sdk_input_is_mouse_move(const input_event_t *event) {
    if (event == NULL) {
        return FALSE;
    }
    return event->type == INPUT_EVENT_MOUSE_MOVE;
}

BOOLEAN sdk_input_is_mouse_click(const input_event_t *event) {
    if (event == NULL) {
        return FALSE;
    }
    return event->type == INPUT_EVENT_MOUSE_BUTTON_DOWN;
}

BOOLEAN sdk_input_is_mouse_release(const input_event_t *event) {
    if (event == NULL) {
        return FALSE;
    }
    return event->type == INPUT_EVENT_MOUSE_BUTTON_UP;
}

// ============================================================================
// Character Input (Keyboard)
// ============================================================================

BOOLEAN sdk_input_get_char(const input_event_t *event, CHAR16 *out_char) {
    if (event == NULL || !sdk_input_is_key_down(event) || out_char == NULL) {
        return FALSE;
    }

    // Printable ASCII range (32-126 in CHAR16)
    CHAR16 ch = event->data.key.unicode;
    if (ch >= 32 && ch <= 126) {
        *out_char = ch;
        return TRUE;
    }

    // Extended characters (let through)
    if (ch > 126) {
        *out_char = ch;
        return TRUE;
    }

    return FALSE;
}

BOOLEAN sdk_input_is_printable(const input_event_t *event) {
    if (event == NULL || !sdk_input_is_key_down(event)) {
        return FALSE;
    }

    CHAR16 ch = event->data.key.unicode;
    // Printable ASCII + extended chars, but not control chars
    return ch >= 32 && ch != 0x7F;
}

BOOLEAN sdk_input_is_backspace(const input_event_t *event) {
    if (event == NULL || !sdk_input_is_key_down(event)) {
        return FALSE;
    }
    // Backspace is scan code 0x08 or unicode 8
    return event->data.key.scan_code == 0x08 || event->data.key.unicode == 8;
}

BOOLEAN sdk_input_is_enter(const input_event_t *event) {
    if (event == NULL || !sdk_input_is_key_down(event)) {
        return FALSE;
    }
    // Enter is typically unicode 13
    return event->data.key.unicode == 13;
}

BOOLEAN sdk_input_is_escape(const input_event_t *event) {
    if (event == NULL || !sdk_input_is_key_down(event)) {
        return FALSE;
    }
    // Escape is scan code 0x17
    return event->data.key.scan_code == 0x17;
}

BOOLEAN sdk_input_is_tab(const input_event_t *event) {
    if (event == NULL || !sdk_input_is_key_down(event)) {
        return FALSE;
    }
    // Tab is unicode 9
    return event->data.key.unicode == 9;
}

BOOLEAN sdk_input_is_space(const input_event_t *event) {
    if (event == NULL || !sdk_input_is_key_down(event)) {
        return FALSE;
    }
    return event->data.key.unicode == 32;
}

// ============================================================================
// Navigation Keys (Keyboard)
// ============================================================================

sdk_arrow_dir_t sdk_input_get_arrow(const input_event_t *event) {
    if (event == NULL || !sdk_input_is_key_down(event)) {
        return SDK_ARROW_NONE;
    }

    // UEFI scan codes for arrow keys
    switch (event->data.key.scan_code) {
        case 0x01: return SDK_ARROW_UP;      // EFI_SCAN_UP
        case 0x02: return SDK_ARROW_DOWN;    // EFI_SCAN_DOWN
        case 0x03: return SDK_ARROW_RIGHT;   // EFI_SCAN_RIGHT
        case 0x04: return SDK_ARROW_LEFT;    // EFI_SCAN_LEFT
        default: return SDK_ARROW_NONE;
    }
}

BOOLEAN sdk_input_is_arrow_key(const input_event_t *event) {
    return sdk_input_get_arrow(event) != SDK_ARROW_NONE;
}

// ============================================================================
// Function Keys
// ============================================================================

BOOLEAN sdk_input_is_function_key(const input_event_t *event, INT32 *out_fn_number) {
    if (event == NULL || !sdk_input_is_key_down(event) || out_fn_number == NULL) {
        return FALSE;
    }

    // UEFI function key scan codes: 0x0B (F1) through 0x14 (F12)
    if (event->data.key.scan_code >= 0x0B && event->data.key.scan_code <= 0x14) {
        *out_fn_number = event->data.key.scan_code - 0x0A;  // F1 = 1, F2 = 2, etc.
        return TRUE;
    }

    return FALSE;
}

BOOLEAN sdk_input_is_delete_key(const input_event_t *event) {
    if (event == NULL || !sdk_input_is_key_down(event)) {
        return FALSE;
    }
    return event->data.key.scan_code == 0x0E;  // EFI_SCAN_DELETE
}

BOOLEAN sdk_input_is_home_key(const input_event_t *event) {
    if (event == NULL || !sdk_input_is_key_down(event)) {
        return FALSE;
    }
    return event->data.key.scan_code == 0x05;  // EFI_SCAN_HOME
}

BOOLEAN sdk_input_is_end_key(const input_event_t *event) {
    if (event == NULL || !sdk_input_is_key_down(event)) {
        return FALSE;
    }
    return event->data.key.scan_code == 0x06;  // EFI_SCAN_END
}

BOOLEAN sdk_input_is_page_up(const input_event_t *event) {
    if (event == NULL || !sdk_input_is_key_down(event)) {
        return FALSE;
    }
    return event->data.key.scan_code == 0x09;  // EFI_SCAN_PAGE_UP
}

BOOLEAN sdk_input_is_page_down(const input_event_t *event) {
    if (event == NULL || !sdk_input_is_key_down(event)) {
        return FALSE;
    }
    return event->data.key.scan_code == 0x0A;  // EFI_SCAN_PAGE_DOWN
}

// ============================================================================
// Modifier Keys
// ============================================================================

BOOLEAN sdk_input_has_shift(const input_event_t *event) {
    if (event == NULL || !sdk_input_is_key_down(event)) {
        return FALSE;
    }
    // Best-effort: detect common scan codes for left/right shift.
    // Keyboard driver currently provides scan_code in event->data.key.scan_code.
    // Use conservative checks for common PS/2/UEFI scan codes.
    UINT16 sc = event->data.key.scan_code;
    if (sc == 0x2A || sc == 0x36) { // Left Shift, Right Shift
        return TRUE;
    }
    return FALSE;
}

BOOLEAN sdk_input_has_ctrl(const input_event_t *event) {
    if (event == NULL || !sdk_input_is_key_down(event)) {
        return FALSE;
    }
    // Detect common control scan code
    UINT16 sc = event->data.key.scan_code;
    if (sc == 0x1D) { // Control
        return TRUE;
    }
    return FALSE;
}

BOOLEAN sdk_input_has_alt(const input_event_t *event) {
    if (event == NULL || !sdk_input_is_key_down(event)) {
        return FALSE;
    }
    // Detect common alt scan code
    UINT16 sc = event->data.key.scan_code;
    if (sc == 0x38) { // Alt
        return TRUE;
    }
    return FALSE;
}

// ============================================================================
// Mouse Input
// ============================================================================

BOOLEAN sdk_input_get_mouse_pos(const input_event_t *event, INT32 *out_x, INT32 *out_y) {
    if (event == NULL || out_x == NULL || out_y == NULL) {
        return FALSE;
    }

    if (sdk_input_is_mouse_move(event) || sdk_input_is_mouse_click(event) || sdk_input_is_mouse_release(event)) {
        *out_x = (INT32)event->data.mouse_move.x;
        *out_y = (INT32)event->data.mouse_move.y;
        
        // Update cached mouse state
        g_last_mouse_state.x = *out_x;
        g_last_mouse_state.y = *out_y;
        
        return TRUE;
    }

    return FALSE;
}

BOOLEAN sdk_input_is_left_click(const input_event_t *event) {
    if (event == NULL || event->type != INPUT_EVENT_MOUSE_BUTTON_DOWN) {
        return FALSE;
    }
    return event->data.mouse_button.left;  // Left button pressed
}

BOOLEAN sdk_input_is_right_click(const input_event_t *event) {
    if (event == NULL || event->type != INPUT_EVENT_MOUSE_BUTTON_DOWN) {
        return FALSE;
    }
    return event->data.mouse_button.right;  // Right button pressed
}

BOOLEAN sdk_input_is_middle_click(const input_event_t *event) {
    if (event == NULL || event->type != INPUT_EVENT_MOUSE_BUTTON_DOWN) {
        return FALSE;
    }
    // Kernel doesn't track middle button separately, only left/right
    return FALSE;
}

BOOLEAN sdk_input_is_left_pressed(const input_event_t *event) {
    if (event == NULL) {
        return FALSE;
    }

    if (event->type == INPUT_EVENT_MOUSE_BUTTON_DOWN && event->data.mouse_button.left) {
        g_last_mouse_state.left_pressed = TRUE;
        return TRUE;
    }
    
    if (event->type == INPUT_EVENT_MOUSE_BUTTON_UP && event->data.mouse_button.left == FALSE) {
        g_last_mouse_state.left_pressed = FALSE;
        return FALSE;
    }

    return g_last_mouse_state.left_pressed;
}

BOOLEAN sdk_input_is_right_pressed(const input_event_t *event) {
    if (event == NULL) {
        return FALSE;
    }

    if (event->type == INPUT_EVENT_MOUSE_BUTTON_DOWN && event->data.mouse_button.right) {
        g_last_mouse_state.right_pressed = TRUE;
        return TRUE;
    }
    
    if (event->type == INPUT_EVENT_MOUSE_BUTTON_UP && event->data.mouse_button.right == FALSE) {
        g_last_mouse_state.right_pressed = FALSE;
        return FALSE;
    }

    return g_last_mouse_state.right_pressed;
}

// ============================================================================
// Hit Testing
// ============================================================================

BOOLEAN sdk_input_hit_test_rect(const input_event_t *event, INT32 x, INT32 y, INT32 w, INT32 h) {
    if (event == NULL || w <= 0 || h <= 0) {
        return FALSE;
    }

    INT32 mx, my;
    if (!sdk_input_get_mouse_pos(event, &mx, &my)) {
        return FALSE;
    }

    return mx >= x && mx < x + w && my >= y && my < y + h;
}

BOOLEAN sdk_input_hit_test_circle(const input_event_t *event, INT32 cx, INT32 cy, INT32 radius) {
    if (event == NULL || radius <= 0) {
        return FALSE;
    }

    INT32 mx, my;
    if (!sdk_input_get_mouse_pos(event, &mx, &my)) {
        return FALSE;
    }

    INT32 dx = mx - cx;
    INT32 dy = my - cy;
    INT32 dist_sq = dx * dx + dy * dy;
    INT32 radius_sq = radius * radius;

    return dist_sq <= radius_sq;
}

// ============================================================================
// Mouse State Tracking
// ============================================================================

sdk_mouse_state_t sdk_get_last_mouse_state(void) {
    return g_last_mouse_state;
}
