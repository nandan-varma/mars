#include "framebuffer.h"
#include "font8x16.h"

static framebuffer_t g_framebuffer;

void framebuffer_init(const boot_info_t *boot_info) {
    g_framebuffer.base = (UINT32 *)(UINTN)boot_info->framebuffer_base;
    g_framebuffer.width = boot_info->width;
    g_framebuffer.height = boot_info->height;
    g_framebuffer.pitch = boot_info->pixels_per_scanline;
}

void drawPixel(INT32 x, INT32 y, UINT32 color) {
    if (x < 0 || y < 0) {
        return;
    }
    if ((UINT32)x >= g_framebuffer.width || (UINT32)y >= g_framebuffer.height) {
        return;
    }

    UINTN offset = (UINTN)y * g_framebuffer.pitch + (UINTN)x;
    g_framebuffer.base[offset] = color;
}

void drawRect(INT32 x, INT32 y, INT32 width, INT32 height, UINT32 color) {
    if (width <= 0 || height <= 0) {
        return;
    }

    for (INT32 row = 0; row < height; ++row) {
        INT32 py = y + row;
        if (py < 0 || (UINT32)py >= g_framebuffer.height) {
            continue;
        }

        for (INT32 col = 0; col < width; ++col) {
            INT32 px = x + col;
            if (px < 0 || (UINT32)px >= g_framebuffer.width) {
                continue;
            }
            UINTN offset = (UINTN)py * g_framebuffer.pitch + (UINTN)px;
            g_framebuffer.base[offset] = color;
        }
    }
}

void clearScreen(UINT32 color) {
    drawRect(0, 0, (INT32)g_framebuffer.width, (INT32)g_framebuffer.height, color);
}

void framebuffer_present(void) {
    return;
}

void drawChar(INT32 x, INT32 y, CHAR16 c, UINT32 fg, UINT32 bg) {
    const UINT8 *glyph = font8x16_get(c);

    for (INT32 row = 0; row < 16; ++row) {
        UINT8 bits = glyph[row];
        for (INT32 col = 0; col < 8; ++col) {
            UINT32 color = (bits & (0x80 >> col)) ? fg : bg;
            drawPixel(x + col, y + row, color);
        }
    }
}

void drawString(INT32 x, INT32 y, const CHAR16 *text, UINT32 fg, UINT32 bg) {
    INT32 cursor_x = x;
    INT32 cursor_y = y;

    for (UINTN index = 0; text[index] != 0; ++index) {
        CHAR16 ch = text[index];

        if (ch == L'\n') {
            cursor_x = x;
            cursor_y += 16;
            continue;
        }

        drawChar(cursor_x, cursor_y, ch, fg, bg);
        cursor_x += 8;
    }
}
