#ifndef MEMORY_H
#define MEMORY_H

#include "platform.h"

void memory_init(const platform_context_t *platform);
EFI_PHYSICAL_ADDRESS memory_alloc_pages(UINTN page_count);
BOOLEAN memory_release_pages(EFI_PHYSICAL_ADDRESS address, UINTN page_count);
UINTN memory_total_pages(void);
UINTN memory_free_pages(void);

#endif
