#include "internal/wm_internal.h"

#include "diag.h"
#include "framebuffer.h"
#include "heap.h"
#include "input.h"
#include "memory.h"
#include "platform.h"
#include "process.h"
#include "scheduler.h"
#include "timer.h"
#include "wm_utils.h"

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
    wm_state_t *state = wm_state();

    UINTN hz = timer_hz();
    if (hz == 0) {
        hz = 1;
    }

    UINT64 sec = timer_ticks() / (UINT64)hz;
    if (state->fallback_clock_hz != hz) {
        state->fallback_clock_hz = hz;
        state->fallback_clock_second = sec;
    } else {
        state->fallback_clock_second = sec;
    }
    return state->fallback_clock_second % 86400ULL;
}

UINT64 wm_current_second_of_day(void) {
    EFI_TIME time;
    UINT64 second = 0;
    if (query_wall_clock(&time, &second)) {
        return second;
    }
    return fallback_second_of_day();
}

void wm_format_clock_text(CHAR16 *out, UINTN max_chars) {
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

static void render_debug_overlay(void) {
    wm_state_t *state = wm_state();

    CHAR16 text[96];
    CHAR16 value[24];

    text[0] = 0;
    wm_append_text(text, 96, L"FPS ");
    wm_to_decimal(state->fps_value, value, 24);
    wm_append_text(text, 96, value);
    wm_append_text(text, 96, L"  PROC ");
    wm_to_decimal((UINT64)process_running_count(), value, 24);
    wm_append_text(text, 96, value);
    wm_append_text(text, 96, L"  TASK ");
    wm_to_decimal((UINT64)scheduler_task_count(), value, 24);
    wm_append_text(text, 96, value);
    drawRect(8, 8, 356, 108, 0x00151F2E);
    drawRect(9, 9, 354, 106, 0x00111A27);
    drawString(16, 14, text, 0x00D7E6F8, 0x00111A27);

    text[0] = 0;
    wm_append_text(text, 96, L"Tick ");
    wm_to_decimal(timer_ticks(), value, 24);
    wm_append_text(text, 96, value);
    wm_append_text(text, 96, L"  Heap ");
    wm_to_decimal((UINT64)(heap_used_bytes() / 1024), value, 24);
    wm_append_text(text, 96, value);
    wm_append_text(text, 96, L"/");
    wm_to_decimal((UINT64)(heap_total_bytes() / 1024), value, 24);
    wm_append_text(text, 96, value);
    wm_append_text(text, 96, L" KB");
    drawString(16, 32, text, 0x00A8C8E8, 0x00111A27);

    text[0] = 0;
    wm_append_text(text, 96, L"Drops ch=");
    wm_to_decimal(event_bus_channel_drop_count(), value, 24);
    wm_append_text(text, 96, value);
    wm_append_text(text, 96, L" pid=");
    wm_to_decimal(event_bus_process_drop_count(), value, 24);
    wm_append_text(text, 96, value);
    drawString(16, 50, text, 0x0096BAD9, 0x00111A27);

    text[0] = 0;
    wm_append_text(text, 96, L"Depth in=");
    wm_to_decimal((UINT64)event_bus_channel_depth(EVENT_CHANNEL_INPUT), value, 24);
    wm_append_text(text, 96, value);
    wm_append_text(text, 96, L" key=");
    wm_to_decimal((UINT64)event_bus_channel_depth(EVENT_CHANNEL_INPUT_KEYBOARD), value, 24);
    wm_append_text(text, 96, value);
    wm_append_text(text, 96, L" sys=");
    wm_to_decimal((UINT64)event_bus_channel_depth(EVENT_CHANNEL_SYSTEM), value, 24);
    wm_append_text(text, 96, value);
    drawString(16, 66, text, 0x0087B4D8, 0x00111A27);

    text[0] = 0;
    wm_append_text(text, 96, L"Stage ");
    wm_to_decimal((UINT64)diag_stage(), value, 24);
    wm_append_text(text, 96, value);
    wm_append_text(text, 96, L"  Pg ");
    wm_to_decimal((UINT64)memory_outstanding_pages(), value, 24);
    wm_append_text(text, 96, value);
    wm_append_text(text, 96, L" a=");
    wm_to_decimal((UINT64)memory_page_alloc_count(), value, 24);
    wm_append_text(text, 96, value);
    wm_append_text(text, 96, L" f=");
    wm_to_decimal((UINT64)memory_page_free_count(), value, 24);
    wm_append_text(text, 96, value);
    drawString(16, 82, text, 0x007AA6CB, 0x00111A27);

    text[0] = 0;
    wm_append_text(text, 96, L"Input st=");
    wm_to_decimal((UINT64)input_lifecycle_state(), value, 24);
    wm_append_text(text, 96, value);
    wm_append_text(text, 96, L" p=");
    wm_to_decimal(input_published_count(), value, 24);
    wm_append_text(text, 96, value);
    wm_append_text(text, 96, L" d=");
    wm_to_decimal(input_drop_count(), value, 24);
    wm_append_text(text, 96, value);
    drawString(16, 98, text, 0x006B9ABF, 0x00111A27);
}

static void render_desktop_background(void) {
    wm_state_t *state = wm_state();

    UINT32 top = 0x00081224;
    UINT32 bottom = 0x00112E52;
    UINT32 h = state->desktop_h > TASKBAR_H ? (state->desktop_h - TASKBAR_H) : state->desktop_h;

    for (UINT32 y = 0; y < h; ++y) {
        UINT32 line_color = rgb_blend(top, bottom, y, h == 0 ? 1 : h);
        drawRect(0, (INT32)y, (INT32)state->desktop_w, 1, line_color);
    }

    for (UINT32 y = 60; y + 2 < h; y += 44) {
        for (UINT32 x = 40; x + 2 < state->desktop_w; x += 44) {
            drawRect((INT32)x, (INT32)y, 2, 2, 0x00173A61);
        }
    }
}

static void render_window(const wm_window_t *window) {
    wm_state_t *state = wm_state();

    if (window == NULL || !window->visible) {
        return;
    }

    if (window->owner_pid != 0 && !process_is_running(window->owner_pid)) {
        wm_window_t *owned_window = wm_find_window(window->id);
        if (owned_window != NULL) {
            owned_window->visible = FALSE;
            owned_window->invalidated = TRUE;
            if (state->focused_window == owned_window->id) {
                state->focused_window = 0;
                state->active_window = 0;
            }
            state->dirty = TRUE;
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

    UINTN index = wm_window_index_by_id(window->id);
    if (index < state->window_count && state->content[index][0] != 0) {
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
                for (UINTN i = 0; i < WM_CONTENT_CHARS && state->content[index][i] != 0; ++i) {
                    CHAR16 ch = state->content[index][i];
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

#define WM_CURSOR_W 12
#define WM_CURSOR_H 19

static void render_mouse_cursor(INT32 hx, INT32 hy) {
    /*
     * macOS-style arrow cursor bitmap (12x19)
     * 0 = transparent, 1 = white border, 2 = black fill
     */
    static const UINT8 cursor[19][12] = {
        {1,0,0,0,0,0,0,0,0,0,0,0},
        {1,1,0,0,0,0,0,0,0,0,0,0},
        {1,2,1,0,0,0,0,0,0,0,0,0},
        {1,2,2,1,0,0,0,0,0,0,0,0},
        {1,2,2,2,1,0,0,0,0,0,0,0},
        {1,2,2,2,2,1,0,0,0,0,0,0},
        {1,2,2,2,2,2,1,0,0,0,0,0},
        {1,2,2,2,2,2,2,1,0,0,0,0},
        {1,2,2,2,2,2,2,2,1,0,0,0},
        {1,2,2,2,2,2,2,2,2,1,0,0},
        {1,2,2,2,2,2,2,2,2,2,1,0},
        {1,2,2,2,2,2,2,1,1,1,1,1},
        {1,2,2,2,1,2,2,1,0,0,0,0},
        {1,2,2,1,0,1,2,2,1,0,0,0},
        {1,2,1,0,0,1,2,2,1,0,0,0},
        {1,1,0,0,0,0,1,2,2,1,0,0},
        {1,0,0,0,0,0,1,2,2,1,0,0},
        {0,0,0,0,0,0,0,1,2,2,1,0},
        {0,0,0,0,0,0,0,1,1,1,0,0},
    };

    for (INT32 y = 0; y < 19; ++y) {
        for (INT32 x = 0; x < 12; ++x) {
            UINT8 p = cursor[y][x];
            if (p == 1) {
                drawPixel(hx + x, hy + y, 0x00FFFFFF);
            } else if (p == 2) {
                drawPixel(hx + x, hy + y, 0x00000000);
            }
        }
    }
}

void wm_render(void) {
    wm_state_t *state = wm_state();

    BOOLEAN clock_only = state->clock_only_redraw;
    UINT64 now = timer_ticks();
    state->clock_last_second = wm_current_second_of_day();
    ++state->fps_counter;
    if (state->fps_last_tick == 0) {
        state->fps_last_tick = now;
    } else if (now > state->fps_last_tick && (now - state->fps_last_tick) >= 1000) {
        state->fps_value = state->fps_counter;
        state->fps_counter = 0;
        state->fps_last_tick = now;
    }

    if (!clock_only) {
        render_desktop_background();
    }
    BOOLEAN rendered_flags[WM_MAX_WINDOWS];
    for (UINTN i = 0; i < WM_MAX_WINDOWS; ++i) {
        rendered_flags[i] = FALSE;
    }

    if (!clock_only) {
        for (UINTN rendered = 0; rendered < state->window_count; ++rendered) {
            UINTN best_index = state->window_count;
            UINT8 best_z = 0xFF;
            for (UINTN i = 0; i < state->window_count; ++i) {
                if (!state->windows[i].visible || rendered_flags[i] || state->windows[i].z > best_z) {
                    continue;
                }

                best_z = state->windows[i].z;
                best_index = i;
            }

            if (best_index < state->window_count) {
                render_window(&state->windows[best_index]);
                state->windows[best_index].invalidated = FALSE;
                rendered_flags[best_index] = TRUE;
            }
        }
    }

    INT32 taskbar_y = (INT32)state->desktop_h - TASKBAR_H;
    UINT32 taskbar_bg = 0x001C2738;
    UINT32 start_bg = state->start_menu_open ? 0x004E8BCE : 0x00315FAA;

    drawRect(0, taskbar_y, (INT32)state->desktop_w, TASKBAR_H, taskbar_bg);
    drawRect(0, taskbar_y, (INT32)state->desktop_w, 1, 0x004A5A70);
    drawRect(6, taskbar_y + 4, START_BUTTON_W, START_BUTTON_H, start_bg);
    drawRect(7, taskbar_y + 5, START_BUTTON_W - 2, START_BUTTON_H - 2, state->start_menu_open ? 0x005B98DB : 0x003B6CB8);
    drawString(20, taskbar_y + 7, L"Start", 0x00FFFFFF, state->start_menu_open ? 0x005B98DB : 0x003B6CB8);
    drawString(90, taskbar_y + 8, L"Mars Desktop", 0x00CFE1F6, taskbar_bg);

    CHAR16 time_text[16];
    wm_format_clock_text(time_text, 16);
    drawRect((INT32)state->desktop_w - 92, taskbar_y + 4, 84, START_BUTTON_H, 0x002A3548);
    drawString((INT32)state->desktop_w - 84, taskbar_y + 8, time_text, 0x00DDEBFF, 0x002A3548);

    if (state->show_debug_overlay) {
        render_debug_overlay();
    }

    if (!clock_only) {
        wm_render_start_menu();
    }

    INT32 cursor_x = input_mouse_x();
    INT32 cursor_y = input_mouse_y();
    render_mouse_cursor(cursor_x, cursor_y);
    if (clock_only) {
        framebuffer_present_region(0, taskbar_y, (INT32)state->desktop_w, TASKBAR_H);
        if (state->show_debug_overlay) {
            framebuffer_present_region(8, 8, 356, 108);
        }
        framebuffer_present_region(cursor_x - 1, cursor_y - 1, WM_CURSOR_W + 2, WM_CURSOR_H + 2);
    } else {
        framebuffer_present();
    }
    state->dirty = FALSE;
    state->clock_only_redraw = FALSE;
}

BOOLEAN wm_needs_redraw(void) {
    wm_state_t *state = wm_state();

    if (state->dirty) {
        state->clock_only_redraw = FALSE;
        return TRUE;
    }

    UINT64 second = wm_current_second_of_day();
    if (second != state->clock_last_second) {
        state->dirty = TRUE;
        state->clock_only_redraw = TRUE;
        return TRUE;
    }

    for (UINTN i = 0; i < state->window_count; ++i) {
        if (state->windows[i].invalidated) {
            state->clock_only_redraw = FALSE;
            return TRUE;
        }
    }

    return FALSE;
}
