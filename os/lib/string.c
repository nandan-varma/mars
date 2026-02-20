#include "os_string.h"

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
