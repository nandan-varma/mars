#ifndef MEMORY_H
#define MEMORY_H

#include "boot_info.h"

void memory_init(const boot_info_t *boot_info);
EFI_PHYSICAL_ADDRESS memory_alloc_pages(UINTN page_count);
UINTN memory_total_pages(void);
UINTN memory_free_pages(void);

#endif
