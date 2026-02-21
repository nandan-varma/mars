#include "wm_internal.h"

#include "os_string.h"
#include "process.h"
#include "timer.h"
#include "wm_utils.h"

static wm_state_t g_state;

static const start_item_t g_start_items[START_MENU_ITEM_COUNT] = {
    { L"calculator", L"Calculator" },
    { L"paint", L"Paint" },
    { L"editor", L"Text Editor" },
    { L"file_manager", L"File Manager" },
    { L"settings", L"Settings" },
    { L"system_monitor", L"System Monitor" }
};

wm_state_t *wm_state(void) {
    return &g_state;
}

const start_item_t *wm_start_items(UINTN *count) {
    if (count != NULL) {
        *count = START_MENU_ITEM_COUNT;
    }
    return g_start_items;
}

void wm_publish_launch_request(const CHAR16 *app_id) {
    if (app_id == NULL) {
        return;
    }

    event_packet_t packet;
    packet.channel = EVENT_CHANNEL_SYSTEM;
    packet.code = EVENT_CODE_APP_LAUNCH_REQUEST;
    packet.source_pid = 0;
    packet.target_pid = 0;
    packet.target_window = 0;

    UINTN id_chars = wm_text_len16(app_id, 24);
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

wm_window_t *wm_find_window(UINT32 id) {
    wm_state_t *state = wm_state();
    for (UINTN i = 0; i < state->window_count; ++i) {
        if (state->windows[i].id == id) {
            return &state->windows[i];
        }
    }
    return NULL;
}

UINTN wm_window_index_by_id(UINT32 id) {
    wm_state_t *state = wm_state();
    for (UINTN i = 0; i < state->window_count; ++i) {
        if (state->windows[i].id == id) {
            return i;
        }
    }

    return state->window_count;
}

static BOOLEAN wm_point_in(INT32 x, INT32 y, const wm_window_t *window) {
    return x >= window->x && y >= window->y && x < window->x + window->width && y < window->y + window->height;
}

void wm_focus_window_internal(UINT32 id) {
    wm_state_t *state = wm_state();
    wm_window_t *focused = wm_find_window(id);
    if (focused == NULL) {
        return;
    }

    for (UINTN i = 0; i < state->window_count; ++i) {
        state->windows[i].focused = FALSE;
    }
    focused->focused = TRUE;
    if (state->next_z == 0) {
        state->next_z = 1;
    }
    focused->z = state->next_z++;
    focused->visible = TRUE;
    state->focused_window = id;
    state->active_window = id;
    state->dirty = TRUE;
}

wm_window_t *wm_top_window_at(INT32 mouse_x, INT32 mouse_y) {
    wm_state_t *state = wm_state();
    wm_window_t *top = NULL;
    UINT8 top_z = 0;

    for (UINTN i = 0; i < state->window_count; ++i) {
        if (!state->windows[i].visible || !wm_point_in(mouse_x, mouse_y, &state->windows[i])) {
            continue;
        }

        if (top == NULL || state->windows[i].z >= top_z) {
            top = &state->windows[i];
            top_z = state->windows[i].z;
        }
    }

    return top;
}

void wm_init(UINT32 desktop_w, UINT32 desktop_h) {
    wm_state_t *state = wm_state();
    state->window_count = 0;
    state->next_window_id = 1;
    state->focused_window = 0;
    state->desktop_w = desktop_w;
    state->desktop_h = desktop_h;
    state->dirty = TRUE;
    state->dragging = FALSE;
    state->resizing = FALSE;
    state->active_window = 0;
    state->drag_offset_x = 0;
    state->drag_offset_y = 0;
    state->next_z = 1;
    state->start_menu_open = FALSE;
    state->fps_last_tick = 0;
    state->fps_counter = 0;
    state->fps_value = 0;
    state->show_debug_overlay = TRUE;
    state->clock_last_second = (UINT64)-1;
    state->clock_only_redraw = FALSE;
    state->fallback_clock_second = 0;
    state->fallback_clock_hz = timer_hz();

    for (UINTN i = 0; i < WM_MAX_WINDOWS; ++i) {
        state->content[i][0] = 0;
    }
}

UINT32 wm_create_window(UINT32 owner_pid, const CHAR16 *title, INT32 x, INT32 y, INT32 width, INT32 height) {
    wm_state_t *state = wm_state();
    if (state->window_count >= WM_MAX_WINDOWS || width < 120 || height < 80) {
        return 0;
    }

    wm_window_t *window = &state->windows[state->window_count++];
    window->id = state->next_window_id++;
    window->owner_pid = owner_pid;
    window->x = wm_clamp_i32(x, 0, (INT32)state->desktop_w - width);
    window->y = wm_clamp_i32(y, 0, (INT32)state->desktop_h - height);
    window->width = width;
    window->height = height;
    window->z = state->next_z++;
    window->focused = FALSE;
    window->visible = TRUE;
    window->invalidated = TRUE;
    os_strcpy16(window->title, title, 32);
    wm_focus_window_internal(window->id);
    return window->id;
}

UINT32 wm_focused_window(void) {
    return wm_state()->focused_window;
}

BOOLEAN wm_focus_window(UINT32 window_id) {
    wm_window_t *window = wm_find_window(window_id);
    if (window == NULL) {
        return FALSE;
    }

    window->visible = TRUE;
    wm_focus_window_internal(window_id);
    return TRUE;
}

BOOLEAN wm_set_window_content(UINT32 window_id, const CHAR16 *text) {
    if (text == NULL) {
        return FALSE;
    }

    wm_state_t *state = wm_state();
    UINTN index = wm_window_index_by_id(window_id);
    if (index >= state->window_count) {
        return FALSE;
    }

    // SECURITY FIX #7: Re-validate window ID and index after lookup
    // Window could have closed between wm_window_index_by_id() and here
    if (index >= WM_MAX_WINDOWS || state->windows[index].id != window_id) {
        return FALSE;
    }

    os_strcpy16(state->content[index], text, WM_CONTENT_CHARS);
    state->windows[index].invalidated = TRUE;
    state->dirty = TRUE;
    return TRUE;
}

void wm_set_debug_overlay(BOOLEAN enabled) {
    wm_state()->show_debug_overlay = enabled;
    wm_state()->dirty = TRUE;
}

BOOLEAN wm_debug_overlay_enabled(void) {
    return wm_state()->show_debug_overlay;
}
