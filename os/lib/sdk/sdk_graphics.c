#include "sdk/sdk_graphics.h"
#include "framebuffer.h"
#include "font8x16.h"
#include "sdk/sdk_core.h"

// ============================================================================
// Clip Region (for window bounds)
// ============================================================================

static BOOLEAN g_clip_enabled = FALSE;
static INT32 g_clip_x = 0;
static INT32 g_clip_y = 0;
static INT32 g_clip_width = 0;
static INT32 g_clip_height = 0;

// ============================================================================
// Graphics Initialization
// ============================================================================

void sdk_graphics_init(void) {
    // Framebuffer is already initialized by kernel
    g_clip_enabled = FALSE;
}

// ============================================================================
// Clip Region
// ============================================================================

void sdk_graphics_set_clip(INT32 x, INT32 y, INT32 width, INT32 height) {
    if (width <= 0 || height <= 0) {
        g_clip_enabled = FALSE;
        return;
    }
    g_clip_x = x;
    g_clip_y = y;
    g_clip_width = width;
    g_clip_height = height;
    g_clip_enabled = TRUE;
}

void sdk_graphics_clear_clip(void) {
    g_clip_enabled = FALSE;
}

// ============================================================================
// Basic Drawing Operations
// ============================================================================

// Helper: Check if point is in clip region
static BOOLEAN sdk_is_clipped(INT32 x, INT32 y) {
    if (!g_clip_enabled) {
        return FALSE;
    }
    return (x < g_clip_x || x >= g_clip_x + g_clip_width ||
            y < g_clip_y || y >= g_clip_y + g_clip_height);
}

// Helper: Clamp rectangle to clip region
static void sdk_clamp_rect(INT32 *x, INT32 *y, INT32 *w, INT32 *h) {
    if (!g_clip_enabled) {
        return;
    }
    INT32 start_x = *x;
    INT32 start_y = *y;
    INT32 end_x = start_x + *w;
    INT32 end_y = start_y + *h;
    
    // Clamp to clip bounds
    if (start_x < g_clip_x) start_x = g_clip_x;
    if (start_y < g_clip_y) start_y = g_clip_y;
    if (end_x > g_clip_x + g_clip_width) end_x = g_clip_x + g_clip_width;
    if (end_y > g_clip_y + g_clip_height) end_y = g_clip_y + g_clip_height;
    
    *x = start_x;
    *y = start_y;
    *w = end_x - start_x;
    *h = end_y - start_y;
}

void sdk_graphics_clear(UINT32 color) {
    if (g_clip_enabled) {
        // Clear only the clip region
        drawRect(g_clip_x, g_clip_y, g_clip_width, g_clip_height, color);
    } else {
        clearScreen(color);
    }
}

void sdk_graphics_pixel(INT32 x, INT32 y, UINT32 color) {
    if (sdk_is_clipped(x, y)) {
        return;
    }
    drawPixel(x, y, color);
}

void sdk_graphics_rect(INT32 x, INT32 y, INT32 w, INT32 h, UINT32 color) {
    if (w <= 0 || h <= 0) {
        return;
    }
    sdk_clamp_rect(&x, &y, &w, &h);
    if (w <= 0 || h <= 0) {
        return;
    }
    drawRect(x, y, w, h, color);
}

void sdk_graphics_rect_outline(INT32 x, INT32 y, INT32 w, INT32 h, UINT32 color, INT32 thickness) {
    if (thickness <= 0) {
        return;
    }

    // Top and bottom borders
    drawRect(x, y, w, thickness, color);
    drawRect(x, y + h - thickness, w, thickness, color);

    // Left and right borders
    drawRect(x, y + thickness, thickness, h - 2 * thickness, color);
    drawRect(x + w - thickness, y + thickness, thickness, h - 2 * thickness, color);
}

void sdk_graphics_line(INT32 x1, INT32 y1, INT32 x2, INT32 y2, UINT32 color) {
    // Bresenham's line algorithm
    INT32 dx = x2 - x1;
    INT32 dy = y2 - y1;
    INT32 steps = sdk_abs(dx) > sdk_abs(dy) ? sdk_abs(dx) : sdk_abs(dy);

    if (steps == 0) {
        drawPixel(x1, y1, color);
        return;
    }

    // Use fixed-point arithmetic to avoid floating point
    // Multiply by 256 for precision
    INT32 x = x1 << 8;  // x1 * 256
    INT32 y = y1 << 8;  // y1 * 256
    INT32 x_inc = (dx << 8) / steps;  // (dx * 256) / steps
    INT32 y_inc = (dy << 8) / steps;  // (dy * 256) / steps

    for (INT32 i = 0; i <= steps; i++) {
        drawPixel(x >> 8, y >> 8, color);
        x += x_inc;
        y += y_inc;
    }
}

void sdk_graphics_circle(INT32 cx, INT32 cy, INT32 radius, UINT32 color) {
    if (radius <= 0) {
        return;
    }

    // Midpoint circle algorithm
    INT32 x = 0;
    INT32 y = radius;
    INT32 d = 3 - 2 * radius;

    while (x <= y) {
        // Draw 8 symmetric points
        drawPixel(cx + x, cy + y, color);
        drawPixel(cx - x, cy + y, color);
        drawPixel(cx + x, cy - y, color);
        drawPixel(cx - x, cy - y, color);
        drawPixel(cx + y, cy + x, color);
        drawPixel(cx - y, cy + x, color);
        drawPixel(cx + y, cy - x, color);
        drawPixel(cx - y, cy - x, color);

        // Fill horizontal lines for each row
        drawRect(cx - x, cy + y, 2 * x + 1, 1, color);
        drawRect(cx - x, cy - y, 2 * x + 1, 1, color);
        drawRect(cx - y, cy + x, 2 * y + 1, 1, color);
        drawRect(cx - y, cy - x, 2 * y + 1, 1, color);

        if (d < 0) {
            d = d + 4 * x + 6;
        } else {
            d = d + 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
}

void sdk_graphics_circle_outline(INT32 cx, INT32 cy, INT32 radius, UINT32 color, INT32 thickness) {
    if (radius <= 0 || thickness <= 0) {
        return;
    }

    for (INT32 i = 0; i < thickness; i++) {
        sdk_graphics_circle(cx, cy, radius - i, color);
    }
}

// ============================================================================
// Advanced Shapes
// ============================================================================

void sdk_graphics_rounded_rect(INT32 x, INT32 y, INT32 w, INT32 h, INT32 radius, UINT32 color) {
    if (w <= 0 || h <= 0 || radius <= 0) {
        return;
    }

    radius = sdk_min(radius, sdk_min(w / 2, h / 2));

    // Draw four straight edges
    drawRect(x + radius, y, w - 2 * radius, radius, color);              // Top
    drawRect(x + radius, y + h - radius, w - 2 * radius, radius, color); // Bottom
    drawRect(x, y + radius, radius, h - 2 * radius, color);              // Left
    drawRect(x + w - radius, y + radius, radius, h - 2 * radius, color); // Right

    // Center rectangle
    drawRect(x + radius, y + radius, w - 2 * radius, h - 2 * radius, color);

    // Draw four corners (quarter circles)
    sdk_graphics_circle(x + radius, y + radius, radius, color);
    sdk_graphics_circle(x + w - radius, y + radius, radius, color);
    sdk_graphics_circle(x + radius, y + h - radius, radius, color);
    sdk_graphics_circle(x + w - radius, y + h - radius, radius, color);
}

void sdk_graphics_triangle(INT32 x1, INT32 y1, INT32 x2, INT32 y2, INT32 x3, INT32 y3, UINT32 color) {
    // Simple scanline triangle fill
    INT32 min_x = sdk_min(x1, sdk_min(x2, x3));
    INT32 max_x = sdk_max(x1, sdk_max(x2, x3));
    INT32 min_y = sdk_min(y1, sdk_min(y2, y3));
    INT32 max_y = sdk_max(y1, sdk_max(y2, y3));

    for (INT32 y = min_y; y <= max_y; y++) {
        // Find intersections with triangle edges
        INT32 left = max_x + 1;
        INT32 right = min_x - 1;

        // Check edge 1-2
        if ((y1 <= y && y <= y2) || (y2 <= y && y <= y1)) {
            if (y1 != y2) {
                INT32 x = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
                left = sdk_min(left, x);
                right = sdk_max(right, x);
            }
        }

        // Check edge 2-3
        if ((y2 <= y && y <= y3) || (y3 <= y && y <= y2)) {
            if (y2 != y3) {
                INT32 x = x2 + (x3 - x2) * (y - y2) / (y3 - y2);
                left = sdk_min(left, x);
                right = sdk_max(right, x);
            }
        }

        // Check edge 3-1
        if ((y3 <= y && y <= y1) || (y1 <= y && y <= y3)) {
            if (y3 != y1) {
                INT32 x = x3 + (x1 - x3) * (y - y3) / (y1 - y3);
                left = sdk_min(left, x);
                right = sdk_max(right, x);
            }
        }

        if (left <= right) {
            drawRect(left, y, right - left + 1, 1, color);
        }
    }
}

// ============================================================================
// Text Rendering
// ============================================================================

void sdk_graphics_text(INT32 x, INT32 y, const CHAR16 *text, UINT32 fg_color, UINT32 bg_color) {
    if (text == NULL) {
        return;
    }

    INT32 cur_x = x;
    for (UINTN i = 0; text[i] != L'\0'; i++) {
        // Skip characters outside clip region
        if (!sdk_is_clipped(cur_x, y) && !sdk_is_clipped(cur_x + SDK_FONT_WIDTH - 1, y + SDK_FONT_HEIGHT - 1)) {
            drawChar(cur_x, y, text[i], fg_color, bg_color);
        }
        cur_x += SDK_FONT_WIDTH;
    }
}

void sdk_graphics_char(INT32 x, INT32 y, CHAR16 ch, UINT32 fg_color, UINT32 bg_color) {
    drawChar(x, y, ch, fg_color, bg_color);
}

UINT32 sdk_graphics_text_width(const CHAR16 *text) {
    if (text == NULL) {
        return 0;
    }

    UINT32 width = 0;
    for (UINTN i = 0; text[i] != L'\0'; i++) {
        width += SDK_FONT_WIDTH;
    }
    return width;
}

UINT32 sdk_graphics_text_height(void) {
    return SDK_FONT_HEIGHT;
}

// ============================================================================
// Visual Effects
// ============================================================================

void sdk_graphics_gradient_rect(INT32 x, INT32 y, INT32 w, INT32 h, UINT32 color_start, UINT32 color_end) {
    if (w <= 0 || h <= 0) {
        return;
    }

    for (INT32 i = 0; i < w; i++) {
        UINT32 blended = sdk_lerp_color(color_start, color_end, i, w - 1);
        drawRect(x + i, y, 1, h, blended);
    }
}

void sdk_graphics_shadow(INT32 x, INT32 y, INT32 w, INT32 h, INT32 blur_size, UINT32 color) {
    if (w <= 0 || h <= 0 || blur_size <= 0) {
        return;
    }

    // Draw darkening effect expanding outward
    for (INT32 i = 0; i < blur_size; i++) {
        UINT8 alpha = (UINT8)(255 * (blur_size - i) / blur_size / 2);  // Semi-transparent
        UINT32 shadow_color = (alpha << 24) | (color & 0xFFFFFF);
        
        // Draw shadow rectangle with decreasing intensity
        drawRect(x - i, y + h + i, w + 2 * i, blur_size - i, shadow_color);
    }
}

// ============================================================================
// Buffering & Presentation
// ============================================================================

void sdk_graphics_present(void) {
    framebuffer_present();
}

void sdk_graphics_present_region(INT32 x, INT32 y, INT32 w, INT32 h) {
    framebuffer_present_region(x, y, w, h);
}

// ============================================================================
// Utility Functions
// ============================================================================

UINT32 sdk_graphics_get_width(void) {
    // Query actual framebuffer dimensions when available
    UINT32 w = framebuffer_get_width();
    if (w == 0) return 1024; // fallback
    return w;
}

UINT32 sdk_graphics_get_height(void) {
    UINT32 h = framebuffer_get_height();
    if (h == 0) return 768; // fallback
    return h;
}

UINT32 sdk_graphics_blend_color(UINT32 color1, UINT32 color2, UINT8 alpha) {
    return sdk_lerp_color(color1, color2, alpha, 255);
}

UINT32 sdk_graphics_rgb(UINT8 r, UINT8 g, UINT8 b) {
    return 0xFF000000 | ((UINT32)r << 16) | ((UINT32)g << 8) | (UINT32)b;
}

UINT8 sdk_graphics_get_red(UINT32 color) {
    return (UINT8)((color >> 16) & 0xFF);
}

UINT8 sdk_graphics_get_green(UINT32 color) {
    return (UINT8)((color >> 8) & 0xFF);
}

UINT8 sdk_graphics_get_blue(UINT32 color) {
    return (UINT8)(color & 0xFF);
}

UINT8 sdk_graphics_get_alpha(UINT32 color) {
    return (UINT8)((color >> 24) & 0xFF);
}
