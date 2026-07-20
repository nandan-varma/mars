#ifndef OS_STRING_H
#define OS_STRING_H

#include "uefi.h"

void os_strcpy16(CHAR16 *dest, const CHAR16 *src, UINTN max_chars);

// The compiler lowers aggregate initializers/copies to calls to these even in
// freestanding mode (-ffreestanding only exempts us from the *headers*, not
// from the compiler's own codegen expectations), so we must provide them.
void *memcpy(void *dest, const void *src, UINTN n);

#endif
