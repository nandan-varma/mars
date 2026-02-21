#ifndef SDK_INPUT_H
#define SDK_INPUT_H

#include "uefi.h"
#include "input.h"

// ============================================================================
// Input Event Type Detection
// ============================================================================

BOOLEAN sdk_input_is_key_down(const input_event_t *event);
BOOLEAN sdk_input_is_key_up(const input_event_t *event);
BOOLEAN sdk_input_is_mouse_move(const input_event_t *event);
BOOLEAN sdk_input_is_mouse_click(const input_event_t *event);
BOOLEAN sdk_input_is_mouse_release(const input_event_t *event);

// ============================================================================
// Character Input (Keyboard)
// ============================================================================

// Extract printable character from key event
BOOLEAN sdk_input_get_char(const input_event_t *event, CHAR16 *out_char);

// Check if event is a printable character
BOOLEAN sdk_input_is_printable(const input_event_t *event);

// Check for specific keys
BOOLEAN sdk_input_is_backspace(const input_event_t *event);
BOOLEAN sdk_input_is_enter(const input_event_t *event);
BOOLEAN sdk_input_is_escape(const input_event_t *event);
BOOLEAN sdk_input_is_tab(const input_event_t *event);
BOOLEAN sdk_input_is_space(const input_event_t *event);

// ============================================================================
// Navigation Keys (Keyboard)
// ============================================================================

typedef enum {
    SDK_ARROW_NONE,
    SDK_ARROW_UP,
    SDK_ARROW_DOWN,
    SDK_ARROW_LEFT,
    SDK_ARROW_RIGHT
} sdk_arrow_dir_t;

sdk_arrow_dir_t sdk_input_get_arrow(const input_event_t *event);
BOOLEAN sdk_input_is_arrow_key(const input_event_t *event);

// ============================================================================
// Function Keys
// ============================================================================

BOOLEAN sdk_input_is_function_key(const input_event_t *event, INT32 *out_fn_number);
BOOLEAN sdk_input_is_delete_key(const input_event_t *event);
BOOLEAN sdk_input_is_home_key(const input_event_t *event);
BOOLEAN sdk_input_is_end_key(const input_event_t *event);
BOOLEAN sdk_input_is_page_up(const input_event_t *event);
BOOLEAN sdk_input_is_page_down(const input_event_t *event);

// ============================================================================
// Modifier Keys (from Shift State)
// ============================================================================

BOOLEAN sdk_input_has_shift(const input_event_t *event);
BOOLEAN sdk_input_has_ctrl(const input_event_t *event);
BOOLEAN sdk_input_has_alt(const input_event_t *event);

// ============================================================================
// Mouse Input
// ============================================================================

// Extract mouse position from event
BOOLEAN sdk_input_get_mouse_pos(const input_event_t *event, INT32 *out_x, INT32 *out_y);

// Determine click type
BOOLEAN sdk_input_is_left_click(const input_event_t *event);
BOOLEAN sdk_input_is_right_click(const input_event_t *event);
BOOLEAN sdk_input_is_middle_click(const input_event_t *event);

// Determine click state
BOOLEAN sdk_input_is_left_pressed(const input_event_t *event);
BOOLEAN sdk_input_is_right_pressed(const input_event_t *event);

// ============================================================================
// Hit Testing
// ============================================================================

// Test if mouse event hits a rectangular region
BOOLEAN sdk_input_hit_test_rect(const input_event_t *event, INT32 x, INT32 y, INT32 w, INT32 h);

// Test if mouse event hits a circular region
BOOLEAN sdk_input_hit_test_circle(const input_event_t *event, INT32 cx, INT32 cy, INT32 radius);

// ============================================================================
// Mouse State Tracking
// ============================================================================

typedef struct {
    INT32 x, y;
    BOOLEAN left_pressed;
    BOOLEAN right_pressed;
    BOOLEAN middle_pressed;
} sdk_mouse_state_t;

// Get last known mouse state
sdk_mouse_state_t sdk_get_last_mouse_state(void);

// ============================================================================
// Input Utility Macros
// ============================================================================

#define SDK_INPUT_IS_TEXT_INPUT(event) (sdk_input_is_printable(event) || sdk_input_is_backspace(event) || sdk_input_is_enter(event))

#endif // SDK_INPUT_H
