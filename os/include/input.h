#ifndef INPUT_H
#define INPUT_H

#include "uefi.h"
#include "boot_info.h"

typedef enum {
    INPUT_EVENT_NONE = 0,
    INPUT_EVENT_KEY_DOWN,
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

void input_init(const boot_info_t *boot_info, UINT32 screen_w, UINT32 screen_h);
void input_poll(void);
BOOLEAN input_pop_event(input_event_t *out_event);

INT32 input_mouse_x(void);
INT32 input_mouse_y(void);
BOOLEAN input_left_down(void);
BOOLEAN input_has_simple_mouse(void);
BOOLEAN input_has_absolute_mouse(void);
BOOLEAN input_has_ps2_mouse(void);

#endif
