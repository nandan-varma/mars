#ifndef OS_STRING_H
#define OS_STRING_H

#include "uefi.h"

void *os_memset(void *dest, UINT8 value, UINTN size);
void *os_memcpy(void *dest, const void *src, UINTN size);
UINTN os_strlen16(const CHAR16 *str);
void os_strcpy16(CHAR16 *dest, const CHAR16 *src, UINTN max_chars);

#endif
