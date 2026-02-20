#ifndef WM_INTERNAL_H
#define WM_INTERNAL_H

#include "wm.h"

#include "event_bus.h"
#include "input.h"

#define WM_MAX_WINDOWS 16
#define START_BUTTON_W 72
#define START_BUTTON_H 22
#define START_MENU_W 238
#define START_MENU_ITEM_H 26
#define START_MENU_ITEM_COUNT 6
#define WM_POINTER_EVENTS_PER_STEP 16
#define WM_KEY_EVENTS_PER_STEP 8
#define TASKBAR_H 30
#define WM_CONTENT_CHARS 512

typedef struct {
    const CHAR16 *id;
    const CHAR16 *label;
} start_item_t;

typedef struct {
    wm_window_t windows[WM_MAX_WINDOWS];
    UINTN window_count;
    UINT32 next_window_id;
    UINT32 focused_window;
    UINT32 desktop_w;
    UINT32 desktop_h;
    BOOLEAN dirty;
    BOOLEAN dragging;
    BOOLEAN resizing;
    UINT32 active_window;
    INT32 drag_offset_x;
    INT32 drag_offset_y;
    CHAR16 content[WM_MAX_WINDOWS][WM_CONTENT_CHARS];
    UINT8 next_z;
    BOOLEAN start_menu_open;
    UINT64 fps_last_tick;
    UINT32 fps_counter;
    UINT32 fps_value;
    BOOLEAN show_debug_overlay;
    UINT64 clock_last_second;
    BOOLEAN clock_only_redraw;
    UINT64 fallback_clock_second;
    UINTN fallback_clock_hz;
} wm_state_t;

wm_state_t *wm_state(void);
const start_item_t *wm_start_items(UINTN *count);

void wm_publish_launch_request(const CHAR16 *app_id);
wm_window_t *wm_find_window(UINT32 id);
UINTN wm_window_index_by_id(UINT32 id);
void wm_focus_window_internal(UINT32 id);
wm_window_t *wm_top_window_at(INT32 mouse_x, INT32 mouse_y);

BOOLEAN wm_handle_start_menu_click(INT32 mouse_x, INT32 mouse_y);
void wm_render_start_menu(void);
UINT64 wm_current_second_of_day(void);
void wm_format_clock_text(CHAR16 *out, UINTN max_chars);

#endif
