#ifndef SDK_CORE_H
#define SDK_CORE_H

#include "uefi.h"

// ============================================================================
// Color Palette (ARGB format - 32-bit)
// ============================================================================

#define SDK_COLOR_BLACK         0xFF000000
#define SDK_COLOR_WHITE         0xFFFFFFFF
#define SDK_COLOR_RED           0xFFFF0000
#define SDK_COLOR_GREEN         0xFF00AA00
#define SDK_COLOR_BLUE          0xFF0000FF
#define SDK_COLOR_YELLOW        0xFFFFFF00
#define SDK_COLOR_CYAN          0xFF00FFFF
#define SDK_COLOR_MAGENTA       0xFFFF00FF
#define SDK_COLOR_GRAY          0xFF808080
#define SDK_COLOR_LIGHT_GRAY    0xFFC0C0C0
#define SDK_COLOR_DARK_GRAY     0xFF404040
#define SDK_COLOR_BG_DARK       0xFF1E1E1E
#define SDK_COLOR_BG_LIGHT      0xFFF5F5F5
#define SDK_COLOR_ACCENT        0xFF0078D4  // Windows blue
#define SDK_COLOR_ACCENT_DARK   0xFF005A9E
#define SDK_COLOR_ACCENT_LIGHT  0xFF107C10  // Green
#define SDK_COLOR_BORDER        0xFFCCCCCC
#define SDK_COLOR_TEXT_PRIMARY  0xFF000000
#define SDK_COLOR_TEXT_SECONDARY 0xFF666666
#define SDK_COLOR_TRANSPARENT   0x00000000

// ============================================================================
// Bounded String Utilities
// ============================================================================

typedef struct {
    CHAR16 *buffer;
    UINTN max_chars;
    UINTN current_len;
} sdk_string_t;

// Initialize a bounded string
sdk_string_t sdk_string_create(CHAR16 *buffer, UINTN max_chars);

// Check if string is at capacity
BOOLEAN sdk_string_is_full(const sdk_string_t *str);

// Append null-terminated string
void sdk_string_append(sdk_string_t *str, const CHAR16 *text);

// Append single character
void sdk_string_append_char(sdk_string_t *str, CHAR16 ch);

// Append integer (base 10)
void sdk_string_append_int(sdk_string_t *str, INT64 value);

// Append unsigned integer (base 10)
void sdk_string_append_uint(sdk_string_t *str, UINT64 value);

// Append integer in hex format (0x...)
void sdk_string_append_hex(sdk_string_t *str, UINT64 value);

// Clear string without deallocating buffer
void sdk_string_clear(sdk_string_t *str);

// Copy from one string to another
void sdk_string_copy(sdk_string_t *dst, const sdk_string_t *src);

// Copy from C-string to sdk_string
void sdk_string_copy_cstr(sdk_string_t *dst, const CHAR16 *src);

// Compare two strings
BOOLEAN sdk_string_equals(const sdk_string_t *a, const sdk_string_t *b);

// Compare string with C-string
BOOLEAN sdk_string_equals_cstr(const sdk_string_t *a, const CHAR16 *b);

// Get current length
UINTN sdk_string_length(const sdk_string_t *str);

// Get remaining capacity
UINTN sdk_string_remaining(const sdk_string_t *str);

// Backspace - remove last character
void sdk_string_backspace(sdk_string_t *str);

// Trim whitespace from end
void sdk_string_trim_end(sdk_string_t *str);

// Convert to uppercase
void sdk_string_to_upper(sdk_string_t *str);

// Convert to lowercase
void sdk_string_to_lower(sdk_string_t *str);

// ============================================================================
// Math Utilities
// ============================================================================

// Clamp value between min and max
INT32 sdk_clamp(INT32 value, INT32 min, INT32 max);

// Absolute value
INT32 sdk_abs(INT32 value);

// Min of two values
INT32 sdk_min(INT32 a, INT32 b);

// Max of two values
INT32 sdk_max(INT32 a, INT32 b);

// Linear interpolation between a and b (0.0 to 1.0)
UINT32 sdk_lerp_color(UINT32 a, UINT32 b, UINT32 factor, UINT32 total);

// ============================================================================
// Geometry Utilities
// ============================================================================

typedef struct {
    INT32 x, y, w, h;
} sdk_rect_t;

typedef struct {
    INT32 x, y;
} sdk_point_t;

// Check if point is inside rectangle
BOOLEAN sdk_rect_contains_point(INT32 rx, INT32 ry, INT32 rw, INT32 rh, INT32 px, INT32 py);

// Check if point is inside rectangle (using struct)
BOOLEAN sdk_rect_contains_point_struct(sdk_rect_t rect, sdk_point_t point);

// Check if two rectangles overlap
BOOLEAN sdk_rect_overlaps(INT32 x1, INT32 y1, INT32 w1, INT32 h1, INT32 x2, INT32 y2, INT32 w2, INT32 h2);

// Check if two rectangles overlap (using structs)
BOOLEAN sdk_rect_overlaps_struct(sdk_rect_t a, sdk_rect_t b);

// Calculate distance between two points
INT32 sdk_distance(INT32 x1, INT32 y1, INT32 x2, INT32 y2);

// Check if point is inside circle
BOOLEAN sdk_circle_contains_point(INT32 cx, INT32 cy, INT32 radius, INT32 px, INT32 py);

// ============================================================================
// Utility Macros
// ============================================================================

#define SDK_ARRAY_SIZE(arr)     (sizeof(arr) / sizeof((arr)[0]))
#define SDK_BYTES_TO_KB(bytes)  ((bytes) / 1024)
#define SDK_BYTES_TO_MB(bytes)  ((bytes) / (1024 * 1024))

#endif // SDK_CORE_H
