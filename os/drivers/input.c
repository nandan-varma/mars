#include "input.h"
#include "keyboard_uefi.h"
#include "mouse_uefi.h"

#define INPUT_QUEUE_CAPACITY 128

static input_event_t g_queue[INPUT_QUEUE_CAPACITY];
static UINTN g_head;
static UINTN g_tail;

static BOOLEAN queue_is_full(void) {
    return ((g_tail + 1) % INPUT_QUEUE_CAPACITY) == g_head;
}

static BOOLEAN queue_is_empty(void) {
    return g_head == g_tail;
}

static void queue_push(const input_event_t *event) {
    if (queue_is_full()) {
        return;
    }

    g_queue[g_tail] = *event;
    g_tail = (g_tail + 1) % INPUT_QUEUE_CAPACITY;
}

static void queue_mouse_move(INT32 dx, INT32 dy) {
    mouse_driver_inject_move(dx, dy);

    input_event_t move_event;
    move_event.type = INPUT_EVENT_MOUSE_MOVE;
    move_event.data.mouse_move.dx = dx;
    move_event.data.mouse_move.dy = dy;
    move_event.data.mouse_move.x = mouse_driver_x();
    move_event.data.mouse_move.y = mouse_driver_y();
    queue_push(&move_event);
}

static void queue_left_button(BOOLEAN down) {
    if (!mouse_driver_set_left(down)) {
        return;
    }

    input_event_t button_event;
    button_event.type = down ? INPUT_EVENT_MOUSE_BUTTON_DOWN : INPUT_EVENT_MOUSE_BUTTON_UP;
    button_event.data.mouse_button.left = down;
    button_event.data.mouse_button.right = FALSE;
    queue_push(&button_event);
}

static void synthesize_mouse_from_key(const input_event_t *key_event) {
    const UINT16 scan = key_event->data.key.scan_code;
    const CHAR16 unicode = key_event->data.key.unicode;
    const INT32 step = 12;

    if (scan == SCAN_LEFT) {
        queue_mouse_move(-step, 0);
    } else if (scan == SCAN_RIGHT) {
        queue_mouse_move(step, 0);
    } else if (scan == SCAN_UP) {
        queue_mouse_move(0, -step);
    } else if (scan == SCAN_DOWN) {
        queue_mouse_move(0, step);
    } else if (unicode == L' ' || unicode == L'\r') {
        queue_left_button(TRUE);
        queue_left_button(FALSE);
    }
}

void input_init(const boot_info_t *boot_info, UINT32 screen_w, UINT32 screen_h) {
    g_head = 0;
    g_tail = 0;

    keyboard_driver_init(boot_info->text_input_ex);
    mouse_driver_init(
        boot_info->boot_services,
        boot_info->simple_pointer,
        boot_info->absolute_pointer,
        screen_w,
        screen_h
    );
}

void input_poll(void) {
    input_event_t event;
    if (keyboard_driver_poll(&event)) {
        queue_push(&event);
        synthesize_mouse_from_key(&event);
    }

    input_event_t mouse_events[3];
    UINTN count = mouse_driver_poll(mouse_events, 3);
    for (UINTN i = 0; i < count; ++i) {
        queue_push(&mouse_events[i]);
    }
}

BOOLEAN input_pop_event(input_event_t *out_event) {
    if (out_event == NULL || queue_is_empty()) {
        return FALSE;
    }

    *out_event = g_queue[g_head];
    g_head = (g_head + 1) % INPUT_QUEUE_CAPACITY;
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
