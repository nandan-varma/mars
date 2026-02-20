#include "os_string.h"

void *os_memset(void *dest, UINT8 value, UINTN size) {
    UINT8 *bytes = (UINT8 *)dest;
    for (UINTN index = 0; index < size; ++index) {
        bytes[index] = value;
    }
    return dest;
}

void *os_memcpy(void *dest, const void *src, UINTN size) {
    UINTN *dst_words = (UINTN *)dest;
    const UINTN *src_words = (const UINTN *)src;

    UINTN word_count = size / sizeof(UINTN);
    for (UINTN index = 0; index < word_count; ++index) {
        dst_words[index] = src_words[index];
    }

    UINTN copied = word_count * sizeof(UINTN);
    UINT8 *dst_bytes = (UINT8 *)dest + copied;
    const UINT8 *src_bytes = (const UINT8 *)src + copied;
    for (UINTN index = copied; index < size; ++index) {
        dst_bytes[index - copied] = src_bytes[index - copied];
    }
    return dest;
}

UINTN os_strlen16(const CHAR16 *str) {
    UINTN len = 0;
    while (str[len] != 0) {
        ++len;
    }
    return len;
}

void os_strcpy16(CHAR16 *dest, const CHAR16 *src, UINTN max_chars) {
    if (max_chars == 0) {
        return;
    }

    UINTN index = 0;
    while (index + 1 < max_chars && src[index] != 0) {
        dest[index] = src[index];
        ++index;
    }
    dest[index] = 0;
}
