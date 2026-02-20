#include "memory.h"

#define PAGE_SIZE 4096
#define MAX_PAGE_REGIONS 128

typedef struct {
    EFI_PHYSICAL_ADDRESS base;
    UINTN pages;
    UINTN used;
} page_region_t;

static page_region_t g_regions[MAX_PAGE_REGIONS];
static UINTN g_region_count;
static UINTN g_total_pages;

static EFI_MEMORY_DESCRIPTOR *nth_descriptor(const platform_context_t *platform, UINTN index) {
    return (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)platform->memory_map.map + index * platform->memory_map.descriptor_size);
}

void memory_init(const platform_context_t *platform) {
    g_region_count = 0;
    g_total_pages = 0;

    if (platform == NULL || platform->memory_map.map == NULL || platform->memory_map.descriptor_size == 0) {
        return;
    }

    UINTN descriptor_count = platform->memory_map.size / platform->memory_map.descriptor_size;

    for (UINTN i = 0; i < descriptor_count; ++i) {
        EFI_MEMORY_DESCRIPTOR *desc = nth_descriptor(platform, i);
        if (desc->Type != EfiConventionalMemory) {
            continue;
        }

        UINTN pages = (UINTN)desc->NumberOfPages;
        if (pages == 0) {
            continue;
        }

        if (g_region_count < MAX_PAGE_REGIONS) {
            g_regions[g_region_count].base = desc->PhysicalStart;
            g_regions[g_region_count].pages = pages;
            g_regions[g_region_count].used = 0;
            g_total_pages += pages;
            ++g_region_count;
        }
    }
}

EFI_PHYSICAL_ADDRESS memory_alloc_pages(UINTN page_count) {
    if (page_count == 0 || g_region_count == 0) {
        return 0;
    }

    for (UINTN i = 0; i < g_region_count; ++i) {
        if (page_count > (g_regions[i].pages - g_regions[i].used)) {
            continue;
        }

        EFI_PHYSICAL_ADDRESS address = g_regions[i].base + (EFI_PHYSICAL_ADDRESS)(g_regions[i].used * PAGE_SIZE);
        g_regions[i].used += page_count;
        return address;
    }

    return 0;
}

UINTN memory_total_pages(void) {
    return g_total_pages;
}

UINTN memory_free_pages(void) {
    UINTN free_pages = 0;

    for (UINTN i = 0; i < g_region_count; ++i) {
        if (g_regions[i].used > g_regions[i].pages) {
            continue;
        }

        free_pages += g_regions[i].pages - g_regions[i].used;
    }

    return free_pages;
}
