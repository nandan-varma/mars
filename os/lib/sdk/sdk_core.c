#include "sdk/sdk_core.h"
#include "os_string.h"

// ============================================================================
// String Operations
// ============================================================================

sdk_string_t sdk_string_create(CHAR16 *buffer, UINTN max_chars) {
    sdk_string_t str;
    str.buffer = buffer;
    str.max_chars = max_chars;
    str.current_len = 0;
    if (buffer != NULL && max_chars > 0) {
        buffer[0] = L'\0';
    }
    return str;
}

BOOLEAN sdk_string_is_full(const sdk_string_t *str) {
    if (str == NULL || str->buffer == NULL) {
        return TRUE;
    }
    return str->current_len >= str->max_chars - 1;
}

void sdk_string_append(sdk_string_t *str, const CHAR16 *text) {
    if (str == NULL || str->buffer == NULL || text == NULL) {
        return;
    }

    UINTN i = 0;
    while (text[i] != L'\0' && !sdk_string_is_full(str)) {
        str->buffer[str->current_len++] = text[i++];
    }
    str->buffer[str->current_len] = L'\0';
}

void sdk_string_append_char(sdk_string_t *str, CHAR16 ch) {
    if (str == NULL || str->buffer == NULL || sdk_string_is_full(str)) {
        return;
    }
    str->buffer[str->current_len++] = ch;
    str->buffer[str->current_len] = L'\0';
}

void sdk_string_append_int(sdk_string_t *str, INT64 value) {
    if (str == NULL || str->buffer == NULL) {
        return;
    }

    if (value < 0) {
        sdk_string_append_char(str, L'-');
        value = -value;
    }

    CHAR16 temp[32];
    UINTN temp_len = 0;

    if (value == 0) {
        sdk_string_append_char(str, L'0');
        return;
    }

    while (value > 0 && temp_len < 31) {
        temp[temp_len++] = L'0' + (CHAR16)(value % 10);
        value /= 10;
    }

    // Reverse and append
    for (UINTN i = temp_len; i > 0; i--) {
        sdk_string_append_char(str, temp[i - 1]);
    }
}

void sdk_string_append_uint(sdk_string_t *str, UINT64 value) {
    if (str == NULL || str->buffer == NULL) {
        return;
    }

    CHAR16 temp[32];
    UINTN temp_len = 0;

    if (value == 0) {
        sdk_string_append_char(str, L'0');
        return;
    }

    while (value > 0 && temp_len < 31) {
        temp[temp_len++] = L'0' + (CHAR16)(value % 10);
        value /= 10;
    }

    // Reverse and append
    for (UINTN i = temp_len; i > 0; i--) {
        sdk_string_append_char(str, temp[i - 1]);
    }
}

void sdk_string_append_hex(sdk_string_t *str, UINT64 value) {
    if (str == NULL || str->buffer == NULL) {
        return;
    }

    sdk_string_append(str, L"0x");

    const CHAR16 *hex_digits = L"0123456789ABCDEF";
    CHAR16 temp[16];
    UINTN temp_len = 0;

    if (value == 0) {
        sdk_string_append_char(str, L'0');
        return;
    }

    while (value > 0 && temp_len < 15) {
        temp[temp_len++] = hex_digits[value & 0xF];
        value >>= 4;
    }

    // Reverse and append
    for (UINTN i = temp_len; i > 0; i--) {
         sdk_string_append_char(str, temp[i - 1]);
     }
 }
 
 void sdk_string_clear(sdk_string_t *str) {
     if (str == NULL || str->buffer == NULL) {
         return;
    }
    str->current_len = 0;
    str->buffer[0] = L'\0';
}

void sdk_string_copy(sdk_string_t *dst, const sdk_string_t *src) {
    if (dst == NULL || src == NULL) {
        return;
    }
    sdk_string_clear(dst);
    if (src->buffer != NULL) {
        sdk_string_append(dst, src->buffer);
    }
}

void sdk_string_copy_cstr(sdk_string_t *dst, const CHAR16 *src) {
    if (dst == NULL) {
        return;
    }
    sdk_string_clear(dst);
    if (src != NULL) {
        sdk_string_append(dst, src);
    }
}

BOOLEAN sdk_string_equals(const sdk_string_t *a, const sdk_string_t *b) {
    if (a == NULL || b == NULL) {
        return a == b;
    }
    if (a->current_len != b->current_len) {
        return FALSE;
    }
    for (UINTN i = 0; i < a->current_len; i++) {
        if (a->buffer[i] != b->buffer[i]) {
            return FALSE;
        }
    }
    return TRUE;
}

BOOLEAN sdk_string_equals_cstr(const sdk_string_t *a, const CHAR16 *b) {
    if (a == NULL || a->buffer == NULL || b == NULL) {
        return a == NULL && b == NULL;
    }

    for (UINTN i = 0; i < a->current_len; i++) {
        if (a->buffer[i] != b[i]) {
            return FALSE;
        }
    }
    return b[a->current_len] == L'\0';
}

UINTN sdk_string_length(const sdk_string_t *str) {
    if (str == NULL) {
        return 0;
    }
    return str->current_len;
}

UINTN sdk_string_remaining(const sdk_string_t *str) {
    if (str == NULL) {
        return 0;
    }
    if (str->current_len >= str->max_chars) {
        return 0;
    }
    return str->max_chars - str->current_len - 1;
}

void sdk_string_backspace(sdk_string_t *str) {
    if (str == NULL || str->current_len == 0) {
        return;
    }
    str->current_len--;
    str->buffer[str->current_len] = L'\0';
}

void sdk_string_trim_end(sdk_string_t *str) {
    if (str == NULL) {
        return;
    }

    while (str->current_len > 0) {
        CHAR16 ch = str->buffer[str->current_len - 1];
        if (ch != L' ' && ch != L'\t' && ch != L'\n' && ch != L'\r') {
            break;
        }
        str->current_len--;
    }
    str->buffer[str->current_len] = L'\0';
}

void sdk_string_to_upper(sdk_string_t *str) {
    if (str == NULL || str->buffer == NULL) {
        return;
    }

    for (UINTN i = 0; i < str->current_len; i++) {
        CHAR16 ch = str->buffer[i];
        if (ch >= L'a' && ch <= L'z') {
            str->buffer[i] = ch - L'a' + L'A';
        }
    }
}

void sdk_string_to_lower(sdk_string_t *str) {
    if (str == NULL || str->buffer == NULL) {
        return;
    }

    for (UINTN i = 0; i < str->current_len; i++) {
        CHAR16 ch = str->buffer[i];
        if (ch >= L'A' && ch <= L'Z') {
            str->buffer[i] = ch - L'A' + L'a';
        }
    }
}

// ============================================================================
// Math Utilities
// ============================================================================

INT32 sdk_clamp(INT32 value, INT32 min, INT32 max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

INT32 sdk_abs(INT32 value) {
    return value < 0 ? -value : value;
}

INT32 sdk_min(INT32 a, INT32 b) {
    return a < b ? a : b;
}

INT32 sdk_max(INT32 a, INT32 b) {
    return a > b ? a : b;
}

UINT32 sdk_lerp_color(UINT32 a, UINT32 b, UINT32 factor, UINT32 total) {
    if (total == 0) {
        return a;
    }

    UINT32 ar = (a >> 16) & 0xFF;
    UINT32 ag = (a >> 8) & 0xFF;
    UINT32 ab = a & 0xFF;
    UINT32 aa = (a >> 24) & 0xFF;

    UINT32 br = (b >> 16) & 0xFF;
    UINT32 bg = (b >> 8) & 0xFF;
    UINT32 bb = b & 0xFF;
    UINT32 ba = (b >> 24) & 0xFF;

    UINT32 rr = (UINT32)((((UINT64)ar * (total - factor)) + ((UINT64)br * factor)) / total);
    UINT32 rg = (UINT32)((((UINT64)ag * (total - factor)) + ((UINT64)bg * factor)) / total);
    UINT32 rb = (UINT32)((((UINT64)ab * (total - factor)) + ((UINT64)bb * factor)) / total);
    UINT32 ra = (UINT32)((((UINT64)aa * (total - factor)) + ((UINT64)ba * factor)) / total);

    return (ra << 24) | (rr << 16) | (rg << 8) | rb;
}

// ============================================================================
// Geometry Utilities
// ============================================================================

BOOLEAN sdk_rect_contains_point(INT32 rx, INT32 ry, INT32 rw, INT32 rh, INT32 px, INT32 py) {
    if (rw <= 0 || rh <= 0) {
        return FALSE;
    }
    return px >= rx && px < rx + rw && py >= ry && py < ry + rh;
}

BOOLEAN sdk_rect_contains_point_struct(sdk_rect_t rect, sdk_point_t point) {
    return sdk_rect_contains_point(rect.x, rect.y, rect.w, rect.h, point.x, point.y);
}

BOOLEAN sdk_rect_overlaps(INT32 x1, INT32 y1, INT32 w1, INT32 h1, INT32 x2, INT32 y2, INT32 w2, INT32 h2) {
    if (w1 <= 0 || h1 <= 0 || w2 <= 0 || h2 <= 0) {
        return FALSE;
    }

    INT32 end_x1 = x1 + w1;
    INT32 end_y1 = y1 + h1;
    INT32 end_x2 = x2 + w2;
    INT32 end_y2 = y2 + h2;

    return x1 < end_x2 && x2 < end_x1 && y1 < end_y2 && y2 < end_y1;
}

BOOLEAN sdk_rect_overlaps_struct(sdk_rect_t a, sdk_rect_t b) {
    return sdk_rect_overlaps(a.x, a.y, a.w, a.h, b.x, b.y, b.w, b.h);
}

INT32 sdk_distance(INT32 x1, INT32 y1, INT32 x2, INT32 y2) {
    INT32 dx = x2 - x1;
    INT32 dy = y2 - y1;
    
    // Simplified distance (Manhattan metric for speed)
    // For pixel-perfect distance, would need sqrt
    return sdk_abs(dx) + sdk_abs(dy);
}

BOOLEAN sdk_circle_contains_point(INT32 cx, INT32 cy, INT32 radius, INT32 px, INT32 py) {
    if (radius <= 0) {
        return FALSE;
    }

    INT32 dx = px - cx;
    INT32 dy = py - cy;
    INT32 dist_sq = dx * dx + dy * dy;
    INT32 radius_sq = radius * radius;

    return dist_sq <= radius_sq;
}
