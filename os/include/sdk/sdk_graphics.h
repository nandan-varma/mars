#ifndef SDK_GRAPHICS_H
#define SDK_GRAPHICS_H

#include "uefi.h"

// ============================================================================
// Graphics Initialization
// ============================================================================

void sdk_graphics_init(void);

// ============================================================================
// Basic Drawing Operations
// ============================================================================

// Clear entire screen to color
void sdk_graphics_clear(UINT32 color);

// Draw single pixel
void sdk_graphics_pixel(INT32 x, INT32 y, UINT32 color);

// Draw filled rectangle
void sdk_graphics_rect(INT32 x, INT32 y, INT32 w, INT32 h, UINT32 color);

// Draw rectangle outline (not filled)
void sdk_graphics_rect_outline(INT32 x, INT32 y, INT32 w, INT32 h, UINT32 color, INT32 thickness);

// Draw line from (x1,y1) to (x2,y2) using Bresenham's algorithm
void sdk_graphics_line(INT32 x1, INT32 y1, INT32 x2, INT32 y2, UINT32 color);

// Draw filled circle
void sdk_graphics_circle(INT32 cx, INT32 cy, INT32 radius, UINT32 color);

// Draw circle outline (not filled)
void sdk_graphics_circle_outline(INT32 cx, INT32 cy, INT32 radius, UINT32 color, INT32 thickness);

// ============================================================================
// Advanced Shapes
// ============================================================================

// Draw filled rectangle with rounded corners
void sdk_graphics_rounded_rect(INT32 x, INT32 y, INT32 w, INT32 h, INT32 radius, UINT32 color);

// Draw filled triangle
void sdk_graphics_triangle(INT32 x1, INT32 y1, INT32 x2, INT32 y2, INT32 x3, INT32 y3, UINT32 color);

// ============================================================================
// Text Rendering (using 8x16 font)
// ============================================================================

#define SDK_FONT_WIDTH  8
#define SDK_FONT_HEIGHT 16

// Draw text at position with foreground and background colors
void sdk_graphics_text(INT32 x, INT32 y, const CHAR16 *text, UINT32 fg_color, UINT32 bg_color);

// Draw single character at position
void sdk_graphics_char(INT32 x, INT32 y, CHAR16 ch, UINT32 fg_color, UINT32 bg_color);

// Get width of text in pixels
UINT32 sdk_graphics_text_width(const CHAR16 *text);

// Get standard text height
UINT32 sdk_graphics_text_height(void);

// ============================================================================
// Visual Effects
// ============================================================================

// Draw gradient rectangle (interpolate between two colors horizontally)
void sdk_graphics_gradient_rect(INT32 x, INT32 y, INT32 w, INT32 h, UINT32 color_start, UINT32 color_end);

// Draw soft shadow (semi-transparent dark area)
void sdk_graphics_shadow(INT32 x, INT32 y, INT32 w, INT32 h, INT32 blur_size, UINT32 color);

// ============================================================================
// Buffering & Presentation
// ============================================================================

// Flush backbuffer to frontbuffer
void sdk_graphics_present(void);

// Present only a region of the backbuffer
void sdk_graphics_present_region(INT32 x, INT32 y, INT32 w, INT32 h);

// ============================================================================
// Utility Functions
// ============================================================================

// Get framebuffer width
UINT32 sdk_graphics_get_width(void);

// Get framebuffer height
UINT32 sdk_graphics_get_height(void);

// Blend two colors with alpha factor (0-255, where 255 = full second color)
UINT32 sdk_graphics_blend_color(UINT32 color1, UINT32 color2, UINT8 alpha);

// Convert RGB to 32-bit color (ARGB)
UINT32 sdk_graphics_rgb(UINT8 r, UINT8 g, UINT8 b);

// Extract red component (0-255)
UINT8 sdk_graphics_get_red(UINT32 color);

// Extract green component (0-255)
UINT8 sdk_graphics_get_green(UINT32 color);

// Extract blue component (0-255)
UINT8 sdk_graphics_get_blue(UINT32 color);

// Extract alpha component (0-255)
UINT8 sdk_graphics_get_alpha(UINT32 color);

#endif // SDK_GRAPHICS_H
