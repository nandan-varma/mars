#ifndef MEMORY_H
#define MEMORY_H

#include "platform.h"

void memory_init(const platform_context_t *platform);
EFI_PHYSICAL_ADDRESS memory_alloc_pages(UINTN page_count);
BOOLEAN memory_release_pages(EFI_PHYSICAL_ADDRESS address, UINTN page_count);
VOID *memory_pool_alloc(UINTN bytes);
BOOLEAN memory_pool_free(VOID *buffer);
UINTN memory_total_pages(void);
UINTN memory_free_pages(void);
UINTN memory_outstanding_pages(void);
UINTN memory_page_alloc_count(void);
UINTN memory_page_free_count(void);
UINTN memory_pool_alloc_count(void);
UINTN memory_pool_free_count(void);

#endif
