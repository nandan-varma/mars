#ifndef WM_H
#define WM_H

#include "uefi.h"

typedef struct {
    UINT32 id;
    UINT32 owner_pid;
    INT32 x;
    INT32 y;
    INT32 width;
    INT32 height;
    UINT8 z;
    BOOLEAN focused;
    BOOLEAN visible;
    BOOLEAN invalidated;
    CHAR16 title[32];
} wm_window_t;

void wm_init(UINT32 desktop_w, UINT32 desktop_h);
UINT32 wm_create_window(UINT32 owner_pid, const CHAR16 *title, INT32 x, INT32 y, INT32 width, INT32 height);
void wm_dispatch_input(void);
void wm_render(void);
BOOLEAN wm_needs_redraw(void);
UINT32 wm_focused_window(void);
BOOLEAN wm_focus_window(UINT32 window_id);
BOOLEAN wm_set_window_content(UINT32 window_id, const CHAR16 *text);
void wm_set_debug_overlay(BOOLEAN enabled);
BOOLEAN wm_debug_overlay_enabled(void);

#endif