#ifndef INPUT_H
#define INPUT_H

#include "uefi.h"
#include "platform.h"

typedef enum {
    INPUT_EVENT_KEY_DOWN = 0,
    INPUT_EVENT_MOUSE_MOVE,
    INPUT_EVENT_MOUSE_BUTTON_DOWN,
    INPUT_EVENT_MOUSE_BUTTON_UP
} input_event_type_t;

typedef struct {
    input_event_type_t type;
    union {
        struct {
            CHAR16 unicode;
            UINT16 scan_code;
        } key;
        struct {
            INT32 dx;
            INT32 dy;
            INT32 x;
            INT32 y;
        } mouse_move;
        struct {
            BOOLEAN left;
            BOOLEAN right;
        } mouse_button;
    } data;
} input_event_t;

typedef enum {
    INPUT_LIFECYCLE_UNINITIALIZED = 0,
    INPUT_LIFECYCLE_PROBED,
    INPUT_LIFECYCLE_STARTED,
    INPUT_LIFECYCLE_STOPPED
} input_lifecycle_state_t;

void input_init(const platform_context_t *platform, UINT32 screen_w, UINT32 screen_h);
void input_stop(void);
void input_poll(void);
BOOLEAN input_pop_event(input_event_t *out_event);
input_lifecycle_state_t input_lifecycle_state(void);
UINT64 input_poll_count(void);
UINT64 input_published_count(void);
UINT64 input_drop_count(void);
UINT64 input_keyboard_poll_count(void);
UINT64 input_mouse_poll_count(void);

INT32 input_mouse_x(void);
INT32 input_mouse_y(void);
BOOLEAN input_left_down(void);
BOOLEAN input_has_simple_mouse(void);
BOOLEAN input_has_absolute_mouse(void);
BOOLEAN input_has_ps2_mouse(void);

#endif
