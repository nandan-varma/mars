#include "wm.h"

#include "event_bus.h"
#include "framebuffer.h"
#include "heap.h"
#include "input.h"
#include "os_string.h"
#include "platform.h"
#include "process.h"
#include "scheduler.h"
#include "timer.h"

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
static CHAR16 g_content[WM_MAX_WINDOWS][WM_CONTENT_CHARS];
static UINT8 g_next_z;
static BOOLEAN g_start_menu_open;
static UINT64 g_fps_last_tick;
static UINT32 g_fps_counter;
static UINT32 g_fps_value;
static BOOLEAN g_show_debug_overlay;
static UINT64 g_clock_last_second;
static UINT64 g_fallback_clock_second;
static UINTN g_fallback_clock_hz;

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

static UINTN text_len16(const CHAR16 *text, UINTN max_chars) {
    if (text == NULL) {
        return 0;
    }

    UINTN len = 0;
    while (len < max_chars && text[len] != 0) {
        ++len;
    }

    return len;
}

static void publish_launch_request(const CHAR16 *app_id) {
    if (app_id == NULL) {
        return;
    }

    event_packet_t packet;
    packet.channel = EVENT_CHANNEL_SYSTEM;
    packet.code = EVENT_CODE_APP_LAUNCH_REQUEST;
    packet.source_pid = 0;
    packet.target_pid = 0;
    packet.target_window = 0;

    UINTN id_chars = text_len16(app_id, 24);
    UINTN id_bytes = (id_chars + 1) * sizeof(CHAR16);
    if (id_bytes > EVENT_PAYLOAD_BYTES) {
        id_bytes = EVENT_PAYLOAD_BYTES;
    }
    packet.payload_size = (UINT32)id_bytes;

    UINTN i = 0;
    for (; i < id_bytes; ++i) {
        packet.payload[i] = ((const UINT8 *)app_id)[i];
    }
    for (; i < EVENT_PAYLOAD_BYTES; ++i) {
        packet.payload[i] = 0;
    }

    (void)event_bus_publish(&packet);
}

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

static UINT32 rgb_blend(UINT32 a, UINT32 b, UINTN num, UINTN den) {
    if (den == 0) {
        return a;
    }

    UINT32 ar = (a >> 16) & 0xFF;
    UINT32 ag = (a >> 8) & 0xFF;
    UINT32 ab = a & 0xFF;

    UINT32 br = (b >> 16) & 0xFF;
    UINT32 bg = (b >> 8) & 0xFF;
    UINT32 bb = b & 0xFF;

    UINT32 rr = (UINT32)((((UINT64)ar * (den - num)) + ((UINT64)br * num)) / den);
    UINT32 rg = (UINT32)((((UINT64)ag * (den - num)) + ((UINT64)bg * num)) / den);
    UINT32 rb = (UINT32)((((UINT64)ab * (den - num)) + ((UINT64)bb * num)) / den);

    return (rr << 16) | (rg << 8) | rb;
}

static void format_two_digits(UINTN value, CHAR16 *out) {
    if (out == NULL) {
        return;
    }

    out[0] = (CHAR16)(L'0' + ((value / 10) % 10));
    out[1] = (CHAR16)(L'0' + (value % 10));
    out[2] = 0;
}

static void format_time(UINT64 ticks, CHAR16 *out, UINTN max_chars) {
    if (out == NULL || max_chars < 9) {
        return;
    }

    UINT64 total_seconds = ticks / 1000;
    UINTN hour = (UINTN)((total_seconds / 3600) % 24);
    UINTN minute = (UINTN)((total_seconds / 60) % 60);
    UINTN second = (UINTN)(total_seconds % 60);

    CHAR16 hh[3];
    CHAR16 mm[3];
    CHAR16 ss[3];
    format_two_digits(hour, hh);
    format_two_digits(minute, mm);
    format_two_digits(second, ss);

    out[0] = hh[0];
    out[1] = hh[1];
    out[2] = L':';
    out[3] = mm[0];
    out[4] = mm[1];
    out[5] = L':';
    out[6] = ss[0];
    out[7] = ss[1];
    out[8] = 0;
}

static BOOLEAN query_wall_clock(EFI_TIME *out_time, UINT64 *out_second_of_day) {
    if (out_time == NULL || out_second_of_day == NULL) {
        return FALSE;
    }

    const platform_context_t *platform = platform_context();
    if (platform == NULL || platform->runtime_services == NULL || platform->runtime_services->GetTime == NULL) {
        return FALSE;
    }

    EFI_TIME time;
    EFI_STATUS status = platform->runtime_services->GetTime(&time, NULL);
    if (EFI_ERROR(status)) {
        return FALSE;
    }

    if (time.Hour > 23 || time.Minute > 59 || time.Second > 59) {
        return FALSE;
    }

    *out_time = time;
    *out_second_of_day = ((UINT64)time.Hour * 3600ULL) + ((UINT64)time.Minute * 60ULL) + (UINT64)time.Second;
    return TRUE;
}

static UINT64 fallback_second_of_day(void) {
    UINTN hz = timer_hz();
    if (hz == 0) {
        hz = 1;
    }

    UINT64 sec = timer_ticks() / (UINT64)hz;
    if (g_fallback_clock_hz != hz) {
        g_fallback_clock_hz = hz;
        g_fallback_clock_second = sec;
    } else {
        g_fallback_clock_second = sec;
    }
    return g_fallback_clock_second % 86400ULL;
}

static UINT64 current_second_of_day(void) {
    EFI_TIME time;
    UINT64 second = 0;
    if (query_wall_clock(&time, &second)) {
        return second;
    }
    return fallback_second_of_day();
}

static void format_clock_text(CHAR16 *out, UINTN max_chars) {
    if (out == NULL || max_chars < 9) {
        return;
    }

    EFI_TIME time;
    UINT64 second = 0;
    if (query_wall_clock(&time, &second)) {
        (void)second;
        CHAR16 hh[3];
        CHAR16 mm[3];
        CHAR16 ss[3];
        format_two_digits((UINTN)time.Hour, hh);
        format_two_digits((UINTN)time.Minute, mm);
        format_two_digits((UINTN)time.Second, ss);
        out[0] = hh[0]; out[1] = hh[1]; out[2] = L':';
        out[3] = mm[0]; out[4] = mm[1]; out[5] = L':';
        out[6] = ss[0]; out[7] = ss[1]; out[8] = 0;
        return;
    }

    format_time(timer_ticks(), out, max_chars);
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
    append_text(text, 96, L"  PROC ");
    to_decimal((UINT64)process_running_count(), value, 24);
    append_text(text, 96, value);
    append_text(text, 96, L"  TASK ");
    to_decimal((UINT64)scheduler_task_count(), value, 24);
    append_text(text, 96, value);
    drawRect(8, 8, 356, 60, 0x00151F2E);
    drawRect(9, 9, 354, 58, 0x00111A27);
    drawString(16, 14, text, 0x00D7E6F8, 0x00111A27);

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
    drawString(16, 32, text, 0x00A8C8E8, 0x00111A27);

    text[0] = 0;
    append_text(text, 96, L"Event drops ch=");
    to_decimal(event_bus_channel_drop_count(), value, 24);
    append_text(text, 96, value);
    append_text(text, 96, L" pid=");
    to_decimal(event_bus_process_drop_count(), value, 24);
    append_text(text, 96, value);
    drawString(16, 50, text, 0x0096BAD9, 0x00111A27);
}

static void render_desktop_background(void) {
    UINT32 top = 0x00081224;
    UINT32 bottom = 0x00112E52;
    UINT32 h = g_desktop_h > TASKBAR_H ? (g_desktop_h - TASKBAR_H) : g_desktop_h;

    for (UINT32 y = 0; y < h; ++y) {
        UINT32 line_color = rgb_blend(top, bottom, y, h == 0 ? 1 : h);
        drawRect(0, (INT32)y, (INT32)g_desktop_w, 1, line_color);
    }

    for (UINT32 y = 60; y + 2 < h; y += 44) {
        for (UINT32 x = 40; x + 2 < g_desktop_w; x += 44) {
            drawRect((INT32)x, (INT32)y, 2, 2, 0x00173A61);
        }
    }
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
    g_show_debug_overlay = TRUE;
    g_clock_last_second = (UINT64)-1;
    g_fallback_clock_second = 0;
    g_fallback_clock_hz = timer_hz();

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
    INT32 taskbar_y = (INT32)g_desktop_h - TASKBAR_H;
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

    INT32 local_y = mouse_y - (menu_y + 28);
    if (local_y < 0) {
        return TRUE;
    }

    UINTN index = (UINTN)(local_y / START_MENU_ITEM_H);
    if (index < START_MENU_ITEM_COUNT) {
        publish_launch_request(g_start_items[index].id);
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

    UINT32 frame_color = window->focused ? 0x0079B7FF : 0x00596A80;
    UINT32 body_color = 0x00E7ECF2;
    UINT32 title_bg = window->focused ? 0x00315FAA : 0x00425065;
    UINT32 title_text = 0x00F4F8FF;

    drawRect(window->x + 4, window->y + 4, window->width, window->height, 0x000B1220);
    drawRect(window->x + 2, window->y + 2, window->width, window->height, 0x00132030);

    drawRect(window->x, window->y, window->width, window->height, frame_color);
    drawRect(window->x + 1, window->y + 1, window->width - 2, window->height - 2, 0x00C8D6E7);
    drawRect(window->x + 2, window->y + 2, window->width - 4, window->height - 4, body_color);
    drawRect(window->x, window->y, window->width, 24, title_bg);
    drawRect(window->x + 2, window->y + 24, window->width - 4, 1, 0x00C0CCDA);
    drawString(window->x + 10, window->y + 6, window->title, title_text, title_bg);

    INT32 min_x = window->x + window->width - 40;
    INT32 min_y = window->y + 4;
    INT32 close_x = window->x + window->width - 20;
    INT32 close_y = window->y + 4;
    drawRect(min_x, min_y, 14, 14, 0x00B1913E);
    drawRect(close_x, close_y, 14, 14, 0x00B84A4A);
    drawString(min_x + 5, min_y + 2, L"_", 0x00FFFFFF, 0x00B1913E);
    drawString(close_x + 4, close_y + 2, L"X", 0x00FFFFFF, 0x00B84A4A);

    drawRect(window->x + window->width - 10, window->y + window->height - 10, 8, 8, 0x0077899E);

    UINTN index = window_index_by_id(window->id);
    if (index < g_window_count && g_content[index][0] != 0) {
        INT32 content_x = window->x + 8;
        INT32 content_y = window->y + 30;
        INT32 content_w = window->width - 16;
        INT32 content_h = window->height - 38;
        if (content_w > 0 && content_h > 0) {
            drawRect(content_x, content_y, content_w, content_h, 0x00EAF0F7);

            INT32 max_cols = content_w / 8;
            INT32 max_rows = content_h / 16;
            if (max_cols > 0 && max_rows > 0) {
                INT32 row = 0;
                INT32 col = 0;
                for (UINTN i = 0; i < WM_CONTENT_CHARS && g_content[index][i] != 0; ++i) {
                    CHAR16 ch = g_content[index][i];
                    if (ch == L'\n') {
                        ++row;
                        col = 0;
                        if (row >= max_rows) {
                            break;
                        }
                        continue;
                    }

                    if (col >= max_cols) {
                        ++row;
                        col = 0;
                        if (row >= max_rows) {
                            break;
                        }
                    }

                    drawChar(content_x + 2 + (col * 8), content_y + 2 + (row * 16), ch, 0x00101A27, 0x00EAF0F7);
                    ++col;
                }
            }
        }
    }
}

static void render_start_menu(void) {
    if (!g_start_menu_open) {
        return;
    }

    INT32 taskbar_y = (INT32)g_desktop_h - TASKBAR_H;
    INT32 menu_x = 6;
    INT32 menu_h = START_MENU_ITEM_COUNT * START_MENU_ITEM_H + 8;
    INT32 menu_y = taskbar_y - menu_h;
    INT32 mouse_x = input_mouse_x();
    INT32 mouse_y = input_mouse_y();

    drawRect(menu_x + 2, menu_y + 2, START_MENU_W, menu_h, 0x00101928);
    drawRect(menu_x, menu_y, START_MENU_W, menu_h, 0x00354456);
    drawRect(menu_x + 1, menu_y + 1, START_MENU_W - 2, menu_h - 2, 0x00EAF0F8);
    drawRect(menu_x + 1, menu_y + 1, START_MENU_W - 2, 24, 0x00315FAA);
    drawString(menu_x + 10, menu_y + 6, L"Applications", 0x00FFFFFF, 0x00315FAA);

    for (UINTN i = 0; i < START_MENU_ITEM_COUNT; ++i) {
        INT32 item_y = menu_y + 28 + (INT32)i * START_MENU_ITEM_H;
        BOOLEAN hovered = point_in_rect(mouse_x, mouse_y, menu_x + 4, item_y, START_MENU_W - 8, START_MENU_ITEM_H - 2);
        UINT32 item_color = hovered ? 0x00CFE0F8 : 0x00DDE7F2;
        UINT32 text_color = hovered ? 0x000B2B55 : 0x00101820;
        drawRect(menu_x + 4, item_y, START_MENU_W - 8, START_MENU_ITEM_H - 2, item_color);
        drawString(menu_x + 10, item_y + 7, g_start_items[i].label, text_color, item_color);
    }
}

void wm_render(void) {
    UINT64 now = timer_ticks();
    g_clock_last_second = current_second_of_day();
    ++g_fps_counter;
    if (g_fps_last_tick == 0) {
        g_fps_last_tick = now;
    } else if (now > g_fps_last_tick && (now - g_fps_last_tick) >= 1000) {
        g_fps_value = g_fps_counter;
        g_fps_counter = 0;
        g_fps_last_tick = now;
    }

    render_desktop_background();
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

    INT32 taskbar_y = (INT32)g_desktop_h - TASKBAR_H;
    UINT32 taskbar_bg = 0x001C2738;
    UINT32 start_bg = g_start_menu_open ? 0x004E8BCE : 0x00315FAA;

    drawRect(0, taskbar_y, (INT32)g_desktop_w, TASKBAR_H, taskbar_bg);
    drawRect(0, taskbar_y, (INT32)g_desktop_w, 1, 0x004A5A70);
    drawRect(6, taskbar_y + 4, START_BUTTON_W, START_BUTTON_H, start_bg);
    drawRect(7, taskbar_y + 5, START_BUTTON_W - 2, START_BUTTON_H - 2, g_start_menu_open ? 0x005B98DB : 0x003B6CB8);
    drawString(20, taskbar_y + 7, L"Start", 0x00FFFFFF, g_start_menu_open ? 0x005B98DB : 0x003B6CB8);
    drawString(90, taskbar_y + 8, L"Mars Desktop", 0x00CFE1F6, taskbar_bg);

    CHAR16 time_text[16];
    format_clock_text(time_text, 16);
    drawRect((INT32)g_desktop_w - 92, taskbar_y + 4, 84, START_BUTTON_H, 0x002A3548);
    drawString((INT32)g_desktop_w - 84, taskbar_y + 8, time_text, 0x00DDEBFF, 0x002A3548);

    if (g_show_debug_overlay) {
        render_debug_overlay();
    }

    render_start_menu();

    drawRect(input_mouse_x(), input_mouse_y(), 6, 10, 0x00FFFFFF);
    drawRect(input_mouse_x() + 1, input_mouse_y() + 1, 4, 8, 0x000B1E38);
    framebuffer_present();
    g_dirty = FALSE;
}

BOOLEAN wm_needs_redraw(void) {
    if (g_dirty) {
        return TRUE;
    }

    UINT64 second = current_second_of_day();
    if (second != g_clock_last_second) {
        g_dirty = TRUE;
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

    os_strcpy16(g_content[index], text, WM_CONTENT_CHARS);
    g_windows[index].invalidated = TRUE;
    g_dirty = TRUE;
    return TRUE;
}

void wm_set_debug_overlay(BOOLEAN enabled) {
    g_show_debug_overlay = enabled;
    g_dirty = TRUE;
}

BOOLEAN wm_debug_overlay_enabled(void) {
    return g_show_debug_overlay;
}