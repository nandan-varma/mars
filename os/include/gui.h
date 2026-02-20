#ifndef GUI_H
#define GUI_H

#include "uefi.h"
#include "boot_info.h"
#include "input.h"

typedef struct {
    INT32 x;
    INT32 y;
    INT32 width;
    INT32 height;
} rect_t;

typedef struct {
    rect_t frame;
    CHAR16 title[32];
} window_t;

typedef struct {
    rect_t frame;
    BOOLEAN pressed;
    CHAR16 label[24];
} button_t;

typedef struct {
    rect_t frame;
    BOOLEAN active;
    CHAR16 text[64];
    UINTN length;
} text_field_t;

void gui_init(const boot_info_t *boot_info);
void gui_handle_event(const input_event_t *event);
void gui_update(void);
void gui_render(void);

#endif
