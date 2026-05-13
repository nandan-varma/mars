#include "app_utils.h"

void app_utils_copy_chars(CHAR16 *dst, const CHAR16 *src, UINTN max_chars) {
    if (max_chars == 0) {
        return;
    }

    UINTN i = 0;
    while (i + 1 < max_chars && src[i] != 0) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = 0;
}

BOOLEAN app_utils_equals(const CHAR16 *a, const CHAR16 *b) {
    UINTN i = 0;
    for (;;) {
        if (a[i] != b[i]) {
            return FALSE;
        }
        if (a[i] == 0) {
            return TRUE;
        }
        ++i;
    }
}

UINTN app_utils_text_length(const CHAR16 *text, UINTN max_chars) {
    if (text == NULL) {
        return 0;
    }

    UINTN len = 0;
    while (len < max_chars && text[len] != 0) {
        ++len;
    }
    return len;
}

void app_utils_buffer_trim_front(CHAR16 *buffer, UINTN max_chars, UINTN *len_io, UINTN needed_space) {
    if (buffer == NULL || len_io == NULL || max_chars == 0) {
        return;
    }

    UINTN len = *len_io;
    while (len + needed_space + 1 >= max_chars && len > 0) {
        UINTN trim = 0;
        while (trim < len && buffer[trim] != L'\n') {
            ++trim;
        }
        if (trim < len && buffer[trim] == L'\n') {
            ++trim;
        }
        if (trim == 0 || trim >= len) {
            len = 0;
            buffer[0] = 0;
            break;
        }

        UINTN write = 0;
        for (UINTN read = trim; read < len; ++read) {
            buffer[write++] = buffer[read];
        }
        len = write;
        buffer[len] = 0;
    }

    *len_io = len;
}

void app_utils_buffer_append_text(CHAR16 *buffer, UINTN max_chars, UINTN *len_io, const CHAR16 *text) {
    if (buffer == NULL || len_io == NULL || text == NULL || max_chars == 0) {
        return;
    }

    UINTN len = *len_io;
    UINTN add = app_utils_text_length(text, max_chars);
    app_utils_buffer_trim_front(buffer, max_chars, &len, add);

    UINTN i = 0;
    while (len + 1 < max_chars && text[i] != 0) {
        buffer[len++] = text[i++];
    }
    buffer[len] = 0;
    *len_io = len;
}

void app_utils_buffer_append_char(CHAR16 *buffer, UINTN max_chars, UINTN *len_io, CHAR16 ch) {
    if (buffer == NULL || len_io == NULL || max_chars == 0) {
        return;
    }

    UINTN len = *len_io;
    app_utils_buffer_trim_front(buffer, max_chars, &len, 1);
    if (len + 1 < max_chars) {
        buffer[len++] = ch;
        buffer[len] = 0;
    }
    *len_io = len;
}

void app_utils_copy_text(CHAR16 *dst, const CHAR16 *src, UINTN max_chars) {
    if (dst == NULL || src == NULL || max_chars == 0) {
        return;
    }

    UINTN i = 0;
    while (i + 1 < max_chars && src[i] != 0) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = 0;
}

void app_utils_copy_append(CHAR16 *dst, UINTN max_chars, const CHAR16 *src) {
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

void app_utils_to_decimal(UINT64 value, CHAR16 *out, UINTN max_chars) {
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

void app_utils_append_u64(CHAR16 *dst, UINTN max_chars, UINT64 value) {
    CHAR16 tmp[24];
    app_utils_to_decimal(value, tmp, 24);
    app_utils_copy_append(dst, max_chars, tmp);
}

void app_utils_to_hex32(UINT32 value, CHAR16 *out, UINTN max_chars) {
    static const CHAR16 digits[] = L"0123456789ABCDEF";
    if (out == NULL || max_chars < 3) {
        return;
    }

    out[0] = L'0';
    out[1] = L'x';
    UINTN written = 2;
    BOOLEAN started = FALSE;

    for (INT32 shift = 28; shift >= 0; shift -= 4) {
        UINT32 nibble = (value >> (UINT32)shift) & 0xFU;
        if (!started && nibble == 0 && shift > 0) {
            continue;
        }
        started = TRUE;
        if (written + 1 >= max_chars) {
            break;
        }
        out[written++] = digits[nibble];
    }

    if (!started && written + 1 < max_chars) {
        out[written++] = L'0';
    }

    out[written] = 0;
}

UINTN app_utils_bounded_len(const CHAR16 *text, UINTN max_chars) {
    UINTN len = 0;
    if (text == NULL) {
        return 0;
    }

    while (len < max_chars && text[len] != 0) {
        ++len;
    }
    return len;
}