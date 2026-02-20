#include "gui.h"
#include "framebuffer.h"
#include "os_string.h"

static window_t g_window;
static button_t g_button;
static text_field_t g_text_field;
static BOOLEAN g_button_flash;

static void append_char(CHAR16 *buffer, UINTN *len, UINTN max, CHAR16 ch) {
    if (*len + 1 >= max) {
        return;
    }
    buffer[*len] = ch;
    *len += 1;
    buffer[*len] = 0;
}

static void append_ascii(CHAR16 *buffer, UINTN *len, UINTN max, const char *text) {
    for (UINTN i = 0; text[i] != 0; ++i) {
        append_char(buffer, len, max, (CHAR16)text[i]);
    }
}

static void append_uint(CHAR16 *buffer, UINTN *len, UINTN max, UINTN value) {
    CHAR16 tmp[21];
    UINTN digits = 0;

    if (value == 0) {
        append_char(buffer, len, max, L'0');
        return;
    }

    while (value > 0 && digits < 20) {
        tmp[digits++] = (CHAR16)(L'0' + (value % 10));
        value /= 10;
    }

    while (digits > 0) {
        append_char(buffer, len, max, tmp[digits - 1]);
        --digits;
    }
}

static BOOLEAN point_in_rect(INT32 x, INT32 y, rect_t rect) {
    return x >= rect.x && y >= rect.y && x < rect.x + rect.width && y < rect.y + rect.height;
}

void gui_init(const boot_info_t *boot_info) {
    (void)boot_info;

    g_window.frame.x = 120;
    g_window.frame.y = 90;
    g_window.frame.width = 480;
    g_window.frame.height = 300;
    os_strcpy16(g_window.title, L"MarsOS Window", 32);

    g_button.frame.x = g_window.frame.x + 24;
    g_button.frame.y = g_window.frame.y + 64;
    g_button.frame.width = 140;
    g_button.frame.height = 36;
    g_button.pressed = FALSE;
    os_strcpy16(g_button.label, L"Click Me", 24);

    g_text_field.frame.x = g_window.frame.x + 24;
    g_text_field.frame.y = g_window.frame.y + 120;
    g_text_field.frame.width = 280;
    g_text_field.frame.height = 32;
    g_text_field.active = FALSE;
    g_text_field.length = 0;
    g_text_field.text[0] = 0;

    g_button_flash = FALSE;
}

void gui_handle_event(const input_event_t *event) {
    if (event == NULL) {
        return;
    }

    INT32 mouse_x = input_mouse_x();
    INT32 mouse_y = input_mouse_y();

    if (event->type == INPUT_EVENT_MOUSE_BUTTON_DOWN && event->data.mouse_button.left) {
        if (point_in_rect(mouse_x, mouse_y, g_button.frame)) {
            g_button.pressed = TRUE;
        }

        g_text_field.active = point_in_rect(mouse_x, mouse_y, g_text_field.frame);
    }

    if (event->type == INPUT_EVENT_MOUSE_BUTTON_UP && !event->data.mouse_button.left) {
        if (g_button.pressed && point_in_rect(mouse_x, mouse_y, g_button.frame)) {
            g_button_flash = !g_button_flash;
        }
        g_button.pressed = FALSE;
    }

    if (event->type == INPUT_EVENT_KEY_DOWN && g_text_field.active) {
        CHAR16 unicode = event->data.key.unicode;
        UINT16 scan_code = event->data.key.scan_code;

        if (scan_code == SCAN_DELETE || unicode == 0x0008) {
            if (g_text_field.length > 0) {
                --g_text_field.length;
                g_text_field.text[g_text_field.length] = 0;
            }
            return;
        }

        if (unicode >= 32 && unicode <= 126 && g_text_field.length + 1 < 64) {
            g_text_field.text[g_text_field.length++] = unicode;
            g_text_field.text[g_text_field.length] = 0;
        }
    }
}

void gui_update(void) {
    if (!input_left_down()) {
        g_button.pressed = FALSE;
    }
}

static void draw_cursor(INT32 x, INT32 y) {
    for (INT32 row = 0; row < 12; ++row) {
        for (INT32 col = 0; col <= row / 2; ++col) {
            drawPixel(x + col, y + row, 0x00FFFFFF);
        }
    }
}

void gui_render(void) {
    UINT32 bg = g_button_flash ? 0x001A2436 : 0x00101820;
    clearScreen(bg);

    drawRect(g_window.frame.x, g_window.frame.y, g_window.frame.width, g_window.frame.height, 0x00D8D8D8);
    drawRect(g_window.frame.x + 1, g_window.frame.y + 1, g_window.frame.width - 2, g_window.frame.height - 2, 0x00F0F0F0);

    drawRect(g_window.frame.x, g_window.frame.y, g_window.frame.width, 28, 0x003070C0);
    drawString(g_window.frame.x + 10, g_window.frame.y + 6, g_window.title, 0x00FFFFFF, 0x003070C0);

    INT32 close_x = g_window.frame.x + g_window.frame.width - 24;
    INT32 close_y = g_window.frame.y + 6;
    drawRect(close_x, close_y, 16, 16, 0x00C04040);
    drawString(close_x + 4, close_y + 2, L"X", 0x00FFFFFF, 0x00C04040);

    UINT32 button_color = g_button.pressed ? 0x006080E0 : 0x0080A0F0;
    drawRect(g_button.frame.x, g_button.frame.y, g_button.frame.width, g_button.frame.height, button_color);
    drawString(g_button.frame.x + 18, g_button.frame.y + 10, g_button.label, 0x00FFFFFF, button_color);

    UINT32 tf_bg = g_text_field.active ? 0x00FFFFFF : 0x00E8E8E8;
    drawRect(g_text_field.frame.x, g_text_field.frame.y, g_text_field.frame.width, g_text_field.frame.height, 0x00404040);
    drawRect(g_text_field.frame.x + 1, g_text_field.frame.y + 1, g_text_field.frame.width - 2, g_text_field.frame.height - 2, tf_bg);
    drawString(g_text_field.frame.x + 6, g_text_field.frame.y + 8, g_text_field.text, 0x00101010, tf_bg);

    drawString(24, 24, L"MarsOS: UEFI kernel online", 0x00FFFFFF, bg);

    CHAR16 debug_line[128];
    UINTN len = 0;
    debug_line[0] = 0;
    append_ascii(debug_line, &len, 128, "MOUSE S:");
    append_ascii(debug_line, &len, 128, input_has_simple_mouse() ? "1" : "0");
    append_ascii(debug_line, &len, 128, " A:");
    append_ascii(debug_line, &len, 128, input_has_absolute_mouse() ? "1" : "0");
    append_ascii(debug_line, &len, 128, " P:");
    append_ascii(debug_line, &len, 128, input_has_ps2_mouse() ? "1" : "0");
    append_ascii(debug_line, &len, 128, " X:");
    append_uint(debug_line, &len, 128, (UINTN)input_mouse_x());
    append_ascii(debug_line, &len, 128, " Y:");
    append_uint(debug_line, &len, 128, (UINTN)input_mouse_y());
    append_ascii(debug_line, &len, 128, " L:");
    append_ascii(debug_line, &len, 128, input_left_down() ? "1" : "0");
    drawString(24, 44, debug_line, 0x00FFFFFF, bg);

    draw_cursor(input_mouse_x(), input_mouse_y());
}
