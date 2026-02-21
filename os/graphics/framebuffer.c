#include "framebuffer.h"
#include "font8x16.h"
#include "memory.h"

#define PAGE_SIZE 4096

static framebuffer_t g_framebuffer;
static UINT32 *g_frontbuffer;
static UINT32 *g_backbuffer;

static UINTN backbuffer_pixel_offset(UINT32 x, UINT32 y) {
    // SECURITY FIX #3: Prevent integer overflow in y * width calculation
    // Check if y * width would overflow by testing if width > max_value / y
    // Since UINTN is 64-bit, max value is 2^64-1
    if (g_framebuffer.width > 0 && y > (18446744073709551615ULL / g_framebuffer.width)) {
        return 18446744073709551615ULL;  // Invalid offset, caller should reject
    }
    return (UINTN)y * g_framebuffer.width + (UINTN)x;
}

void framebuffer_init(const platform_context_t *platform) {
    if (platform == NULL) {
        return;
    }

    g_frontbuffer = (UINT32 *)(UINTN)platform->framebuffer.base;
    g_framebuffer.base = g_frontbuffer;
    g_framebuffer.width = platform->framebuffer.width;
    g_framebuffer.height = platform->framebuffer.height;
    g_framebuffer.pitch = platform->framebuffer.pixels_per_scanline;

    // SECURITY FIX #16: Reject zero dimensions to prevent division by zero
    if (g_framebuffer.width == 0 || g_framebuffer.height == 0 || g_framebuffer.pitch == 0) {
        g_frontbuffer = NULL;
        g_framebuffer.base = NULL;
        return;
    }

    UINTN backbuffer_bytes = (UINTN)g_framebuffer.width * (UINTN)g_framebuffer.height * sizeof(UINT32);
    UINTN pages = (backbuffer_bytes + PAGE_SIZE - 1) / PAGE_SIZE;

    g_backbuffer = NULL;
    EFI_PHYSICAL_ADDRESS backbuffer_physical = memory_alloc_pages(pages);
    if (backbuffer_physical != 0) {
        g_backbuffer = (UINT32 *)(UINTN)backbuffer_physical;
        g_framebuffer.base = g_backbuffer;
    }
}

void drawPixel(INT32 x, INT32 y, UINT32 color) {
    if (x < 0 || y < 0) {
        return;
    }
    if ((UINT32)x >= g_framebuffer.width || (UINT32)y >= g_framebuffer.height) {
        return;
    }

    UINTN offset = g_backbuffer != NULL
        ? backbuffer_pixel_offset((UINT32)x, (UINT32)y)
        : (UINTN)y * g_framebuffer.pitch + (UINTN)x;
    
    // SECURITY FIX: Check if offset calculation resulted in overflow
    // Invalid offset (max uint64) indicates an overflow occurred
    if (offset == 18446744073709551615ULL) {
        return;
    }
    
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
            UINTN offset = g_backbuffer != NULL
                ? backbuffer_pixel_offset((UINT32)px, (UINT32)py)
                : (UINTN)py * g_framebuffer.pitch + (UINTN)px;
            g_framebuffer.base[offset] = color;
        }
    }
}

void clearScreen(UINT32 color) {
    drawRect(0, 0, (INT32)g_framebuffer.width, (INT32)g_framebuffer.height, color);
}

void framebuffer_present(void) {
    if (g_backbuffer == NULL || g_frontbuffer == NULL) {
        return;
    }

    // SECURITY FIX #2: Validate pitch doesn't cause out-of-bounds writes
    // Pitch can be different from width on some hardware
    if (g_framebuffer.pitch < g_framebuffer.width) {
        return;  // Invalid pitch configuration
    }

    for (UINT32 y = 0; y < g_framebuffer.height; ++y) {
        UINTN src_row = (UINTN)y * g_framebuffer.width;
        UINTN dst_row = (UINTN)y * g_framebuffer.pitch;
        for (UINT32 x = 0; x < g_framebuffer.width; ++x) {
            g_frontbuffer[dst_row + x] = g_backbuffer[src_row + x];
        }
    }
}

void framebuffer_present_region(INT32 x, INT32 y, INT32 width, INT32 height) {
    if (g_backbuffer == NULL || g_frontbuffer == NULL || width <= 0 || height <= 0) {
        return;
    }

    INT32 start_x = x < 0 ? 0 : x;
    INT32 start_y = y < 0 ? 0 : y;
    INT32 end_x = x + width;
    INT32 end_y = y + height;

    if (end_x > (INT32)g_framebuffer.width) {
        end_x = (INT32)g_framebuffer.width;
    }
    if (end_y > (INT32)g_framebuffer.height) {
        end_y = (INT32)g_framebuffer.height;
    }

    if (start_x >= end_x || start_y >= end_y) {
        return;
    }

    for (INT32 py = start_y; py < end_y; ++py) {
        UINTN src_row = (UINTN)py * g_framebuffer.width;
        UINTN dst_row = (UINTN)py * g_framebuffer.pitch;
        for (INT32 px = start_x; px < end_x; ++px) {
            g_frontbuffer[dst_row + (UINTN)px] = g_backbuffer[src_row + (UINTN)px];
        }
    }
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
    if (text == NULL) {
        return;
    }

    INT32 cursor_x = x;
    INT32 cursor_y = y;
    UINTN guard = 0;
    UINTN max_chars = 2048;

    for (UINTN index = 0; text[index] != 0 && guard < max_chars; ++index, ++guard) {
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
