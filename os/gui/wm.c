#include "wm.h"

#include "app.h"
#include "event_bus.h"
#include "framebuffer.h"
#include "heap.h"
#include "input.h"
#include "os_string.h"
#include "process.h"
#include "scheduler.h"
#include "timer.h"

#define WM_MAX_WINDOWS 16
#define START_BUTTON_W 72
#define START_BUTTON_H 20
#define START_MENU_W 220
#define START_MENU_ITEM_H 24
#define START_MENU_ITEM_COUNT 6
#define WM_POINTER_EVENTS_PER_STEP 16
#define WM_KEY_EVENTS_PER_STEP 8

static wm_window_t g_windows[WM_MAX_WINDOWS];
static UINTN g_window_count;
static UINT32 g_next_window_id;
static UINT32 g_focused_window;
static UINT32 g_desktop_w;
static UINT32 g_desktop_h;
static BOOLEAN g_dirty;
static BOOLEAN g_dragging;
static BOOLEAN g_resizing;
static UINT32 g_active_window;
static INT32 g_drag_offset_x;
static INT32 g_drag_offset_y;
static CHAR16 g_content[WM_MAX_WINDOWS][96];
static UINT8 g_next_z;
static BOOLEAN g_start_menu_open;
static UINT64 g_fps_last_tick;
static UINT32 g_fps_counter;
static UINT32 g_fps_value;

typedef struct {
    const CHAR16 *id;
    const CHAR16 *label;
} start_item_t;

static const start_item_t g_start_items[START_MENU_ITEM_COUNT] = {
    { L"shell", L"System Shell" },
    { L"files", L"File Browser" },
    { L"term", L"Terminal" },
    { L"settings", L"Settings" },
    { L"tasks", L"Task Manager" },
    { L"logs", L"System Logs" }
};

static void append_text(CHAR16 *dst, UINTN max_chars, const CHAR16 *src) {
    if (dst == NULL || src == NULL || max_chars == 0) {
        return;
    }

    UINTN len = 0;
    while (len + 1 < max_chars && dst[len] != 0) {
        ++len;
    }

    UINTN i = 0;
    while (len + 1 < max_chars && src[i] != 0) {
        dst[len++] = src[i++];
    }
    dst[len] = 0;
}

static void to_decimal(UINT64 value, CHAR16 *out, UINTN max_chars) {
    if (out == NULL || max_chars == 0) {
        return;
    }

    if (value == 0) {
        out[0] = L'0';
        if (max_chars > 1) {
            out[1] = 0;
        }
        return;
    }

    CHAR16 tmp[24];
    UINTN len = 0;
    while (value > 0 && len < 23) {
        tmp[len++] = (CHAR16)(L'0' + (value % 10));
        value /= 10;
    }

    UINTN out_index = 0;
    while (len > 0 && out_index + 1 < max_chars) {
        out[out_index++] = tmp[--len];
    }
    out[out_index] = 0;
}

static void render_debug_overlay(void) {
    CHAR16 text[96];
    CHAR16 value[24];

    text[0] = 0;
    append_text(text, 96, L"FPS ");
    to_decimal(g_fps_value, value, 24);
    append_text(text, 96, value);
    append_text(text, 96, L"  P ");
    to_decimal((UINT64)process_running_count(), value, 24);
    append_text(text, 96, value);
    append_text(text, 96, L"  T ");
    to_decimal((UINT64)scheduler_task_count(), value, 24);
    append_text(text, 96, value);
    drawString(12, 12, text, 0x00D0E8FF, 0x00101820);

    text[0] = 0;
    append_text(text, 96, L"Tick ");
    to_decimal(timer_ticks(), value, 24);
    append_text(text, 96, value);
    append_text(text, 96, L"  Heap ");
    to_decimal((UINT64)(heap_used_bytes() / 1024), value, 24);
    append_text(text, 96, value);
    append_text(text, 96, L"/");
    to_decimal((UINT64)(heap_total_bytes() / 1024), value, 24);
    append_text(text, 96, value);
    append_text(text, 96, L" KB");
    drawString(12, 30, text, 0x00A8C8E8, 0x00101820);
}

static INT32 clamp_i32(INT32 value, INT32 low, INT32 high) {
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

static wm_window_t *find_window(UINT32 id) {
    for (UINTN i = 0; i < g_window_count; ++i) {
        if (g_windows[i].id == id) {
            return &g_windows[i];
        }
    }
    return NULL;
}

static UINTN window_index_by_id(UINT32 id) {
    for (UINTN i = 0; i < g_window_count; ++i) {
        if (g_windows[i].id == id) {
            return i;
        }
    }

    return g_window_count;
}

static BOOLEAN point_in(INT32 x, INT32 y, const wm_window_t *w) {
    return x >= w->x && y >= w->y && x < w->x + w->width && y < w->y + w->height;
}

static BOOLEAN point_in_rect(INT32 x, INT32 y, INT32 rx, INT32 ry, INT32 rw, INT32 rh) {
    return x >= rx && y >= ry && x < rx + rw && y < ry + rh;
}

static void focus_window(UINT32 id) {
    wm_window_t *focused = find_window(id);
    if (focused == NULL) {
        return;
    }

    for (UINTN i = 0; i < g_window_count; ++i) {
        g_windows[i].focused = FALSE;
    }
    focused->focused = TRUE;
    if (g_next_z == 0) {
        g_next_z = 1;
    }
    focused->z = g_next_z++;
    focused->visible = TRUE;
    g_focused_window = id;
    g_active_window = id;
    g_dirty = TRUE;
}

static void forward_input_to_focused_window(const input_event_t *event) {
    if (event == NULL || g_focused_window == 0) {
        return;
    }

    wm_window_t *window = find_window(g_focused_window);
    if (window == NULL || window->owner_pid == 0) {
        return;
    }

    if (!process_is_running(window->owner_pid)) {
        window->visible = FALSE;
        window->invalidated = TRUE;
        g_focused_window = 0;
        g_active_window = 0;
        g_dirty = TRUE;
        return;
    }

    event_packet_t packet;
    packet.channel = EVENT_CHANNEL_APP;
    packet.code = EVENT_CODE_APP_INPUT;
    packet.source_pid = 0;
    packet.target_pid = window->owner_pid;
    packet.target_window = window->id;
    packet.payload_size = sizeof(input_event_t);

    const UINT8 *src = (const UINT8 *)event;
    for (UINTN i = 0; i < sizeof(input_event_t); ++i) {
        packet.payload[i] = src[i];
    }
    for (UINTN i = sizeof(input_event_t); i < EVENT_PAYLOAD_BYTES; ++i) {
        packet.payload[i] = 0;
    }

    (void)event_bus_publish(&packet);
}

void wm_init(UINT32 desktop_w, UINT32 desktop_h) {
    g_window_count = 0;
    g_next_window_id = 1;
    g_focused_window = 0;
    g_desktop_w = desktop_w;
    g_desktop_h = desktop_h;
    g_dirty = TRUE;
    g_dragging = FALSE;
    g_resizing = FALSE;
    g_active_window = 0;
    g_drag_offset_x = 0;
    g_drag_offset_y = 0;
    g_next_z = 1;
    g_start_menu_open = FALSE;
    g_fps_last_tick = 0;
    g_fps_counter = 0;
    g_fps_value = 0;

    for (UINTN i = 0; i < WM_MAX_WINDOWS; ++i) {
        g_content[i][0] = 0;
    }
}

UINT32 wm_create_window(UINT32 owner_pid, const CHAR16 *title, INT32 x, INT32 y, INT32 width, INT32 height) {
    if (g_window_count >= WM_MAX_WINDOWS || width < 120 || height < 80) {
        return 0;
    }

    wm_window_t *window = &g_windows[g_window_count++];
    window->id = g_next_window_id++;
    window->owner_pid = owner_pid;
    window->x = clamp_i32(x, 0, (INT32)g_desktop_w - width);
    window->y = clamp_i32(y, 0, (INT32)g_desktop_h - height);
    window->width = width;
    window->height = height;
    window->z = g_next_z++;
    window->focused = FALSE;
    window->visible = TRUE;
    window->invalidated = TRUE;
    os_strcpy16(window->title, title, 32);
    focus_window(window->id);
    return window->id;
}

static void handle_mouse_move(const input_event_t *event) {
    if (event == NULL || g_active_window == 0) {
        g_dirty = TRUE;
        return;
    }

    wm_window_t *window = find_window(g_active_window);
    if (window == NULL) {
        return;
    }

    if (g_dragging) {
        window->x = clamp_i32(event->data.mouse_move.x - g_drag_offset_x, 0, (INT32)g_desktop_w - window->width);
        window->y = clamp_i32(event->data.mouse_move.y - g_drag_offset_y, 0, (INT32)g_desktop_h - window->height);
        window->invalidated = TRUE;
        g_dirty = TRUE;
    }

    if (g_resizing) {
        INT32 new_w = event->data.mouse_move.x - window->x;
        INT32 new_h = event->data.mouse_move.y - window->y;
        window->width = clamp_i32(new_w, 120, (INT32)g_desktop_w - window->x);
        window->height = clamp_i32(new_h, 80, (INT32)g_desktop_h - window->y);
        window->invalidated = TRUE;
        g_dirty = TRUE;
    }

    g_dirty = TRUE;
}

static wm_window_t *top_window_at(INT32 mouse_x, INT32 mouse_y) {
    wm_window_t *top = NULL;
    UINT8 top_z = 0;

    for (UINTN i = 0; i < g_window_count; ++i) {
        if (!g_windows[i].visible || !point_in(mouse_x, mouse_y, &g_windows[i])) {
            continue;
        }

        if (top == NULL || g_windows[i].z >= top_z) {
            top = &g_windows[i];
            top_z = g_windows[i].z;
        }
    }

    return top;
}

static BOOLEAN handle_start_menu_click(INT32 mouse_x, INT32 mouse_y) {
    INT32 taskbar_y = (INT32)g_desktop_h - 28;
    INT32 start_x = 6;
    INT32 start_y = taskbar_y + 4;

    if (point_in_rect(mouse_x, mouse_y, start_x, start_y, START_BUTTON_W, START_BUTTON_H)) {
        g_start_menu_open = !g_start_menu_open;
        g_dirty = TRUE;
        return TRUE;
    }

    if (!g_start_menu_open) {
        return FALSE;
    }

    INT32 menu_x = 6;
    INT32 menu_h = START_MENU_ITEM_COUNT * START_MENU_ITEM_H + 8;
    INT32 menu_y = taskbar_y - menu_h;

    if (!point_in_rect(mouse_x, mouse_y, menu_x, menu_y, START_MENU_W, menu_h)) {
        g_start_menu_open = FALSE;
        g_dirty = TRUE;
        return FALSE;
    }

    INT32 local_y = mouse_y - (menu_y + 4);
    if (local_y < 0) {
        return TRUE;
    }

    UINTN index = (UINTN)(local_y / START_MENU_ITEM_H);
    if (index < START_MENU_ITEM_COUNT) {
        (void)app_launch(g_start_items[index].id);
        g_start_menu_open = FALSE;
        g_dirty = TRUE;
    }

    return TRUE;
}

static void handle_button_down(void) {
    INT32 mouse_x = input_mouse_x();
    INT32 mouse_y = input_mouse_y();

    if (handle_start_menu_click(mouse_x, mouse_y)) {
        return;
    }

    g_start_menu_open = FALSE;

    wm_window_t *window = top_window_at(mouse_x, mouse_y);
    if (window == NULL) {
        g_focused_window = 0;
        g_active_window = 0;
        g_dragging = FALSE;
        g_resizing = FALSE;
        g_dirty = TRUE;
        return;
    }

    focus_window(window->id);

    INT32 close_x = window->x + window->width - 20;
    INT32 close_y = window->y + 4;
    INT32 min_x = window->x + window->width - 40;
    INT32 min_y = window->y + 4;

    if (point_in_rect(mouse_x, mouse_y, close_x, close_y, 14, 14)) {
        window->visible = FALSE;
        window->invalidated = TRUE;
        process_exit(window->owner_pid, 0);
        if (g_focused_window == window->id) {
            g_focused_window = 0;
            g_active_window = 0;
        }
        g_dirty = TRUE;
        return;
    }

    if (point_in_rect(mouse_x, mouse_y, min_x, min_y, 14, 14)) {
        window->visible = FALSE;
        window->invalidated = TRUE;
        if (g_focused_window == window->id) {
            g_focused_window = 0;
            g_active_window = 0;
        }
        g_dirty = TRUE;
        return;
    }

    if (mouse_y < window->y + 24) {
        g_dragging = TRUE;
        g_active_window = window->id;
        g_drag_offset_x = mouse_x - window->x;
        g_drag_offset_y = mouse_y - window->y;
        g_dirty = TRUE;
    } else if (mouse_x >= window->x + window->width - 12 && mouse_y >= window->y + window->height - 12) {
        g_resizing = TRUE;
        g_active_window = window->id;
        g_dirty = TRUE;
    }
}

static void handle_button_up(void) {
    g_dragging = FALSE;
    g_resizing = FALSE;
}

void wm_dispatch_input(void) {
    event_packet_t packet;

    UINTN pointer_processed = 0;
    while (pointer_processed < WM_POINTER_EVENTS_PER_STEP && event_bus_receive_channel(EVENT_CHANNEL_INPUT, &packet)) {
        if (packet.code != EVENT_CODE_INPUT || packet.payload_size < sizeof(input_event_t)) {
            ++pointer_processed;
            continue;
        }

        input_event_t event;
        UINT8 *dst = (UINT8 *)&event;
        for (UINTN i = 0; i < sizeof(input_event_t); ++i) {
            dst[i] = packet.payload[i];
        }

        if (event.type == INPUT_EVENT_MOUSE_MOVE) {
            handle_mouse_move(&event);
        } else if (event.type == INPUT_EVENT_MOUSE_BUTTON_DOWN) {
            handle_button_down();
            forward_input_to_focused_window(&event);
            g_dirty = TRUE;
        } else if (event.type == INPUT_EVENT_MOUSE_BUTTON_UP) {
            handle_button_up();
            forward_input_to_focused_window(&event);
            g_dirty = TRUE;
        }

        ++pointer_processed;
    }

    UINTN key_processed = 0;
    while (key_processed < WM_KEY_EVENTS_PER_STEP && event_bus_receive_channel(EVENT_CHANNEL_INPUT_KEYBOARD, &packet)) {
        if (packet.code != EVENT_CODE_INPUT || packet.payload_size < sizeof(input_event_t)) {
            ++key_processed;
            continue;
        }

        input_event_t event;
        UINT8 *dst = (UINT8 *)&event;
        for (UINTN i = 0; i < sizeof(input_event_t); ++i) {
            dst[i] = packet.payload[i];
        }

        if (event.type == INPUT_EVENT_KEY_DOWN) {
            forward_input_to_focused_window(&event);
            g_dirty = TRUE;
        }

        ++key_processed;
    }
}

static void render_window(const wm_window_t *window) {
    if (window == NULL || !window->visible) {
        return;
    }

    if (window->owner_pid != 0 && !process_is_running(window->owner_pid)) {
        wm_window_t *owned_window = find_window(window->id);
        if (owned_window != NULL) {
            owned_window->visible = FALSE;
            owned_window->invalidated = TRUE;
            if (g_focused_window == owned_window->id) {
                g_focused_window = 0;
                g_active_window = 0;
            }
            g_dirty = TRUE;
        }
        return;
    }

    UINT32 frame_color = window->focused ? 0x00A0C8FF : 0x00708090;
    UINT32 body_color = 0x00ECEFF4;
    UINT32 title_bg = window->focused ? 0x003067B1 : 0x004A5568;

    drawRect(window->x, window->y, window->width, window->height, frame_color);
    drawRect(window->x + 1, window->y + 1, window->width - 2, window->height - 2, body_color);
    drawRect(window->x, window->y, window->width, 24, title_bg);
    drawString(window->x + 8, window->y + 6, window->title, 0x00FFFFFF, title_bg);

    INT32 min_x = window->x + window->width - 40;
    INT32 min_y = window->y + 4;
    INT32 close_x = window->x + window->width - 20;
    INT32 close_y = window->y + 4;
    drawRect(min_x, min_y, 14, 14, 0x00C0A040);
    drawRect(close_x, close_y, 14, 14, 0x00C04040);
    drawString(min_x + 5, min_y + 2, L"_", 0x00FFFFFF, 0x00C0A040);
    drawString(close_x + 4, close_y + 2, L"X", 0x00FFFFFF, 0x00C04040);

    drawRect(window->x + window->width - 10, window->y + window->height - 10, 8, 8, 0x00607080);

    UINTN index = window_index_by_id(window->id);
    if (index < g_window_count && g_content[index][0] != 0) {
        drawString(window->x + 10, window->y + 34, g_content[index], 0x00101820, body_color);
    }
}

static void render_start_menu(void) {
    if (!g_start_menu_open) {
        return;
    }

    INT32 taskbar_y = (INT32)g_desktop_h - 28;
    INT32 menu_x = 6;
    INT32 menu_h = START_MENU_ITEM_COUNT * START_MENU_ITEM_H + 8;
    INT32 menu_y = taskbar_y - menu_h;

    drawRect(menu_x, menu_y, START_MENU_W, menu_h, 0x00354456);
    drawRect(menu_x + 1, menu_y + 1, START_MENU_W - 2, menu_h - 2, 0x00EDF2F7);

    for (UINTN i = 0; i < START_MENU_ITEM_COUNT; ++i) {
        INT32 item_y = menu_y + 4 + (INT32)i * START_MENU_ITEM_H;
        drawRect(menu_x + 4, item_y, START_MENU_W - 8, START_MENU_ITEM_H - 2, 0x00DDE5EF);
        drawString(menu_x + 10, item_y + 6, g_start_items[i].label, 0x00101820, 0x00DDE5EF);
    }
}

void wm_render(void) {
    UINT64 now = timer_ticks();
    ++g_fps_counter;
    if (g_fps_last_tick == 0) {
        g_fps_last_tick = now;
    } else if (now > g_fps_last_tick && (now - g_fps_last_tick) >= 1000) {
        g_fps_value = g_fps_counter;
        g_fps_counter = 0;
        g_fps_last_tick = now;
    }

    clearScreen(0x00101820);
    BOOLEAN rendered_flags[WM_MAX_WINDOWS];
    for (UINTN i = 0; i < WM_MAX_WINDOWS; ++i) {
        rendered_flags[i] = FALSE;
    }

    for (UINTN rendered = 0; rendered < g_window_count; ++rendered) {
        UINTN best_index = g_window_count;
        UINT8 best_z = 0xFF;
        for (UINTN i = 0; i < g_window_count; ++i) {
            if (!g_windows[i].visible || rendered_flags[i] || g_windows[i].z > best_z) {
                continue;
            }

            best_z = g_windows[i].z;
            best_index = i;
        }

        if (best_index < g_window_count) {
            render_window(&g_windows[best_index]);
            g_windows[best_index].invalidated = FALSE;
            rendered_flags[best_index] = TRUE;
        }
    }

    drawRect(0, (INT32)g_desktop_h - 28, (INT32)g_desktop_w, 28, 0x00222B3A);
    drawRect(6, (INT32)g_desktop_h - 24, START_BUTTON_W, START_BUTTON_H, g_start_menu_open ? 0x005080C0 : 0x003067B1);
    drawString(20, (INT32)g_desktop_h - 19, L"Start", 0x00FFFFFF, g_start_menu_open ? 0x005080C0 : 0x003067B1);
    drawString(90, (INT32)g_desktop_h - 20, L"Mars Desktop", 0x00FFFFFF, 0x00222B3A);

    render_debug_overlay();

    render_start_menu();

    drawRect(input_mouse_x(), input_mouse_y(), 6, 10, 0x00FFFFFF);
    framebuffer_present();
    g_dirty = FALSE;
}

BOOLEAN wm_needs_redraw(void) {
    if (g_dirty) {
        return TRUE;
    }

    for (UINTN i = 0; i < g_window_count; ++i) {
        if (g_windows[i].invalidated) {
            return TRUE;
        }
    }

    return FALSE;
}

UINT32 wm_focused_window(void) {
    return g_focused_window;
}

BOOLEAN wm_focus_window(UINT32 window_id) {
    wm_window_t *window = find_window(window_id);
    if (window == NULL) {
        return FALSE;
    }

    window->visible = TRUE;
    focus_window(window_id);
    return TRUE;
}

BOOLEAN wm_set_window_content(UINT32 window_id, const CHAR16 *text) {
    if (text == NULL) {
        return FALSE;
    }

    UINTN index = window_index_by_id(window_id);
    if (index >= g_window_count) {
        return FALSE;
    }

    os_strcpy16(g_content[index], text, 96);
    g_windows[index].invalidated = TRUE;
    g_dirty = TRUE;
    return TRUE;
}