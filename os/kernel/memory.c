#include "memory.h"

#define PAGE_SIZE 4096

typedef struct {
    EFI_PHYSICAL_ADDRESS base;
    UINTN pages;
    UINTN used;
} page_region_t;

static page_region_t g_region;
static UINTN g_total_pages;

static EFI_MEMORY_DESCRIPTOR *nth_descriptor(const boot_info_t *boot_info, UINTN index) {
    return (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)boot_info->memory_map + index * boot_info->memory_descriptor_size);
}

void memory_init(const boot_info_t *boot_info) {
    g_region.base = 0;
    g_region.pages = 0;
    g_region.used = 0;
    g_total_pages = 0;

    if (boot_info == NULL || boot_info->memory_map == NULL || boot_info->memory_descriptor_size == 0) {
        return;
    }

    UINTN descriptor_count = boot_info->memory_map_size / boot_info->memory_descriptor_size;

    for (UINTN i = 0; i < descriptor_count; ++i) {
        EFI_MEMORY_DESCRIPTOR *desc = nth_descriptor(boot_info, i);
        if (desc->Type != EfiConventionalMemory) {
            continue;
        }

        g_total_pages += (UINTN)desc->NumberOfPages;

        if ((UINTN)desc->NumberOfPages > g_region.pages) {
            g_region.base = desc->PhysicalStart;
            g_region.pages = (UINTN)desc->NumberOfPages;
            g_region.used = 0;
        }
    }
}

EFI_PHYSICAL_ADDRESS memory_alloc_pages(UINTN page_count) {
    if (page_count == 0 || g_region.pages == 0) {
        return 0;
    }

    if (g_region.used + page_count > g_region.pages) {
        return 0;
    }

    EFI_PHYSICAL_ADDRESS address = g_region.base + (EFI_PHYSICAL_ADDRESS)(g_region.used * PAGE_SIZE);
    g_region.used += page_count;
    return address;
}

UINTN memory_total_pages(void) {
    return g_total_pages;
}

UINTN memory_free_pages(void) {
    if (g_region.pages < g_region.used) {
        return 0;
    }

    return g_region.pages - g_region.used;
}
