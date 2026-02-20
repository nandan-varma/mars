#ifndef OS_STRING_H
#define OS_STRING_H

#include "uefi.h"

void os_strcpy16(CHAR16 *dest, const CHAR16 *src, UINTN max_chars);

#endif
