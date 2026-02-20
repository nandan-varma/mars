#include "memory.h"

#define PAGE_SIZE 4096
#define MAX_PAGE_REGIONS 128
#define MAX_PAGE_ALLOCS 256
#define MAX_FREE_BLOCKS 256

typedef struct {
    EFI_PHYSICAL_ADDRESS base;
    UINTN pages;
    UINTN used;
} page_region_t;

typedef struct {
    EFI_PHYSICAL_ADDRESS base;
    UINTN pages;
    BOOLEAN active;
} page_block_t;

static page_region_t g_regions[MAX_PAGE_REGIONS];
static page_block_t g_active_allocs[MAX_PAGE_ALLOCS];
static page_block_t g_free_blocks[MAX_FREE_BLOCKS];
static UINTN g_region_count;
static UINTN g_total_pages;
static EFI_BOOT_SERVICES *g_boot_services;
static UINTN g_outstanding_pages;
static UINTN g_page_alloc_count;
static UINTN g_page_free_count;
static UINTN g_pool_alloc_count;
static UINTN g_pool_free_count;

static EFI_MEMORY_DESCRIPTOR *nth_descriptor(const platform_context_t *platform, UINTN index) {
    return (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)platform->memory_map.map + index * platform->memory_map.descriptor_size);
}

static void clear_blocks(page_block_t *blocks, UINTN count) {
    for (UINTN i = 0; i < count; ++i) {
        blocks[i].base = 0;
        blocks[i].pages = 0;
        blocks[i].active = FALSE;
    }
}

static BOOLEAN track_active_alloc(EFI_PHYSICAL_ADDRESS base, UINTN pages) {
    for (UINTN i = 0; i < MAX_PAGE_ALLOCS; ++i) {
        if (g_active_allocs[i].active) {
            continue;
        }

        g_active_allocs[i].base = base;
        g_active_allocs[i].pages = pages;
        g_active_allocs[i].active = TRUE;
        return TRUE;
    }

    return FALSE;
}

static INTN find_active_alloc(EFI_PHYSICAL_ADDRESS base, UINTN pages) {
    for (UINTN i = 0; i < MAX_PAGE_ALLOCS; ++i) {
        if (!g_active_allocs[i].active) {
            continue;
        }

        if (g_active_allocs[i].base == base && g_active_allocs[i].pages == pages) {
            return (INTN)i;
        }
    }

    return -1;
}

static BOOLEAN push_free_block(EFI_PHYSICAL_ADDRESS base, UINTN pages) {
    for (UINTN i = 0; i < MAX_FREE_BLOCKS; ++i) {
        if (g_free_blocks[i].active) {
            continue;
        }

        g_free_blocks[i].base = base;
        g_free_blocks[i].pages = pages;
        g_free_blocks[i].active = TRUE;
        return TRUE;
    }

    return FALSE;
}

static void merge_free_blocks(void) {
    for (UINTN i = 0; i < MAX_FREE_BLOCKS; ++i) {
        if (!g_free_blocks[i].active) {
            continue;
        }

        EFI_PHYSICAL_ADDRESS i_end = g_free_blocks[i].base + (EFI_PHYSICAL_ADDRESS)(g_free_blocks[i].pages * PAGE_SIZE);

        for (UINTN j = 0; j < MAX_FREE_BLOCKS; ++j) {
            if (i == j || !g_free_blocks[j].active) {
                continue;
            }

            EFI_PHYSICAL_ADDRESS j_end = g_free_blocks[j].base + (EFI_PHYSICAL_ADDRESS)(g_free_blocks[j].pages * PAGE_SIZE);
            if (i_end == g_free_blocks[j].base) {
                g_free_blocks[i].pages += g_free_blocks[j].pages;
                g_free_blocks[j].active = FALSE;
                i_end = g_free_blocks[i].base + (EFI_PHYSICAL_ADDRESS)(g_free_blocks[i].pages * PAGE_SIZE);
            } else if (j_end == g_free_blocks[i].base) {
                g_free_blocks[i].base = g_free_blocks[j].base;
                g_free_blocks[i].pages += g_free_blocks[j].pages;
                g_free_blocks[j].active = FALSE;
                i_end = g_free_blocks[i].base + (EFI_PHYSICAL_ADDRESS)(g_free_blocks[i].pages * PAGE_SIZE);
            }
        }
    }
}

static EFI_PHYSICAL_ADDRESS alloc_from_free_blocks(UINTN page_count) {
    for (UINTN i = 0; i < MAX_FREE_BLOCKS; ++i) {
        if (!g_free_blocks[i].active || g_free_blocks[i].pages < page_count) {
            continue;
        }

        EFI_PHYSICAL_ADDRESS address = g_free_blocks[i].base;
        g_free_blocks[i].base += (EFI_PHYSICAL_ADDRESS)(page_count * PAGE_SIZE);
        g_free_blocks[i].pages -= page_count;
        if (g_free_blocks[i].pages == 0) {
            g_free_blocks[i].active = FALSE;
        }

        return address;
    }

    return 0;
}

void memory_init(const platform_context_t *platform) {
    g_region_count = 0;
    g_total_pages = 0;
    g_boot_services = NULL;
    g_outstanding_pages = 0;
    g_page_alloc_count = 0;
    g_page_free_count = 0;
    g_pool_alloc_count = 0;
    g_pool_free_count = 0;
    clear_blocks(g_active_allocs, MAX_PAGE_ALLOCS);
    clear_blocks(g_free_blocks, MAX_FREE_BLOCKS);

    if (platform == NULL) {
        return;
    }

    g_boot_services = platform->boot_services;

    if (platform->memory_map.map == NULL || platform->memory_map.descriptor_size == 0) {
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

    EFI_PHYSICAL_ADDRESS from_free = alloc_from_free_blocks(page_count);
    if (from_free != 0) {
        if (!track_active_alloc(from_free, page_count)) {
            (void)push_free_block(from_free, page_count);
            merge_free_blocks();
            return 0;
        }

        g_outstanding_pages += page_count;
        ++g_page_alloc_count;
        return from_free;
    }

    for (UINTN i = 0; i < g_region_count; ++i) {
        if (page_count > (g_regions[i].pages - g_regions[i].used)) {
            continue;
        }

        EFI_PHYSICAL_ADDRESS address = g_regions[i].base + (EFI_PHYSICAL_ADDRESS)(g_regions[i].used * PAGE_SIZE);
        if (!track_active_alloc(address, page_count)) {
            return 0;
        }
        g_regions[i].used += page_count;
        g_outstanding_pages += page_count;
        ++g_page_alloc_count;
        return address;
    }

    return 0;
}

BOOLEAN memory_release_pages(EFI_PHYSICAL_ADDRESS address, UINTN page_count) {
    if (address == 0 || page_count == 0) {
        return FALSE;
    }

    INTN index = find_active_alloc(address, page_count);
    if (index < 0) {
        return FALSE;
    }

    g_active_allocs[index].active = FALSE;
    if (!push_free_block(address, page_count)) {
        return FALSE;
    }

    merge_free_blocks();
    if (g_outstanding_pages >= page_count) {
        g_outstanding_pages -= page_count;
    } else {
        g_outstanding_pages = 0;
    }
    ++g_page_free_count;
    return TRUE;
}

VOID *memory_pool_alloc(UINTN bytes) {
    if (bytes == 0 || g_boot_services == NULL) {
        return NULL;
    }

    VOID *buffer = NULL;
    EFI_STATUS status = g_boot_services->AllocatePool(EfiLoaderData, bytes, &buffer);
    if (EFI_ERROR(status)) {
        return NULL;
    }

    ++g_pool_alloc_count;
    return buffer;
}

BOOLEAN memory_pool_free(VOID *buffer) {
    if (buffer == NULL || g_boot_services == NULL) {
        return FALSE;
    }

    EFI_STATUS status = g_boot_services->FreePool(buffer);
    if (EFI_ERROR(status)) {
        return FALSE;
    }

    ++g_pool_free_count;
    return TRUE;
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

UINTN memory_outstanding_pages(void) {
    return g_outstanding_pages;
}

UINTN memory_page_alloc_count(void) {
    return g_page_alloc_count;
}

UINTN memory_page_free_count(void) {
    return g_page_free_count;
}

UINTN memory_pool_alloc_count(void) {
    return g_pool_alloc_count;
}

UINTN memory_pool_free_count(void) {
    return g_pool_free_count;
}
