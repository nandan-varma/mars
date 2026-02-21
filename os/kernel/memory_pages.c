#include "memory_internal.h"

#define MAX_PAGE_REGIONS 128

typedef struct {
    EFI_PHYSICAL_ADDRESS base;
    UINTN pages;
    UINTN used;
} page_region_t;

static page_region_t g_regions[MAX_PAGE_REGIONS];
static UINTN g_region_count;
static UINTN g_total_pages;

void memory_pages_reset(void) {
    g_region_count = 0;
    g_total_pages = 0;
}

void memory_pages_add_region(EFI_PHYSICAL_ADDRESS base, UINTN pages) {
    if (pages == 0 || g_region_count >= MAX_PAGE_REGIONS) {
        return;
    }

    if ((base & (PAGE_SIZE - 1)) != 0) {
        return;
    }

    EFI_PHYSICAL_ADDRESS end = base + (EFI_PHYSICAL_ADDRESS)pages * PAGE_SIZE;
    if (end < base) {
        return;
    }

    for (UINTN i = 0; i < g_region_count; ++i) {
        EFI_PHYSICAL_ADDRESS existing_end = g_regions[i].base + (EFI_PHYSICAL_ADDRESS)g_regions[i].pages * PAGE_SIZE;
        
        // SECURITY FIX #8: Check for overflow when computing existing_end
        // If overflow occurs, existing_end < base, making the overlap check unreliable
        if (existing_end < g_regions[i].base) {
            continue;  // Skip this corrupted region
        }
        
        if (!(end <= g_regions[i].base || base >= existing_end)) {
            return;
        }
    }

    g_regions[g_region_count].base = base;
    g_regions[g_region_count].pages = pages;
    g_regions[g_region_count].used = 0;
    g_total_pages += pages;
    ++g_region_count;
}

EFI_PHYSICAL_ADDRESS memory_pages_alloc_from_regions(UINTN page_count) {
    if (page_count == 0) {
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

UINTN memory_pages_total(void) {
    return g_total_pages;
}

UINTN memory_pages_free_estimate(void) {
    UINTN free_pages = 0;

    for (UINTN i = 0; i < g_region_count; ++i) {
        if (g_regions[i].used > g_regions[i].pages) {
            continue;
        }

        free_pages += g_regions[i].pages - g_regions[i].used;
    }

    return free_pages;
}
