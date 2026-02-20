#ifndef MEMORY_INTERNAL_H
#define MEMORY_INTERNAL_H

#include "memory.h"

#define PAGE_SIZE 4096

void memory_pages_reset(void);
void memory_pages_add_region(EFI_PHYSICAL_ADDRESS base, UINTN pages);
EFI_PHYSICAL_ADDRESS memory_pages_alloc_from_regions(UINTN page_count);
UINTN memory_pages_total(void);
UINTN memory_pages_free_estimate(void);

void memory_freelist_reset(void);
BOOLEAN memory_freelist_track_active(EFI_PHYSICAL_ADDRESS base, UINTN pages);
BOOLEAN memory_freelist_untrack_active(EFI_PHYSICAL_ADDRESS base, UINTN pages);
BOOLEAN memory_freelist_push(EFI_PHYSICAL_ADDRESS base, UINTN pages);
EFI_PHYSICAL_ADDRESS memory_freelist_alloc(UINTN page_count);
void memory_freelist_merge(void);

#endif
