#include "wm_utils.h"

UINTN wm_text_len16(const CHAR16 *text, UINTN max_chars) {
    if (text == NULL) {
        return 0;
    }

    UINTN len = 0;
    while (len < max_chars && text[len] != 0) {
        ++len;
    }

    return len;
}

void wm_append_text(CHAR16 *dst, UINTN max_chars, const CHAR16 *src) {
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

void wm_to_decimal(UINT64 value, CHAR16 *out, UINTN max_chars) {
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

INT32 wm_clamp_i32(INT32 value, INT32 low, INT32 high) {
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

BOOLEAN wm_point_in_rect(INT32 x, INT32 y, INT32 rx, INT32 ry, INT32 rw, INT32 rh) {
    return x >= rx && y >= ry && x < rx + rw && y < ry + rh;
}
