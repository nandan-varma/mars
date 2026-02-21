#ifndef HEAP_H
#define HEAP_H

#include "uefi.h"

void heap_init(UINTN initial_pages);
void *heap_alloc(UINTN size);
void heap_free(void *ptr);
UINTN heap_total_bytes(void);
UINTN heap_used_bytes(void);

#endif