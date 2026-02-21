#include "memory.h"

#include "diag.h"
#include "memory_internal.h"

static EFI_BOOT_SERVICES *g_boot_services;
static UINTN g_outstanding_pages;
static UINTN g_page_alloc_count;
static UINTN g_page_free_count;
static UINTN g_pool_alloc_count;
static UINTN g_pool_free_count;

static EFI_MEMORY_DESCRIPTOR *nth_descriptor(const platform_context_t *platform, UINTN index) {
    return (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)platform->memory_map.map + index * platform->memory_map.descriptor_size);
}

void memory_init(const platform_context_t *platform) {
    g_boot_services = NULL;
    g_outstanding_pages = 0;
    g_page_alloc_count = 0;
    g_page_free_count = 0;
    g_pool_alloc_count = 0;
    g_pool_free_count = 0;

    memory_pages_reset();
    memory_freelist_reset();

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
        // OPTIMIZATION: Early exit if descriptor is not conventional memory and we've already added some regions
        // However, since descriptors may not be sorted, we scan all to ensure no conventional memory is missed
        if (desc->Type != EfiConventionalMemory) {
            continue;
        }

        UINTN pages = (UINTN)desc->NumberOfPages;
        if (pages == 0) {
            continue;
        }

        memory_pages_add_region(desc->PhysicalStart, pages);
    }
}

EFI_PHYSICAL_ADDRESS memory_alloc_pages(UINTN page_count) {
    if (page_count == 0) {
        return 0;
    }

    EFI_PHYSICAL_ADDRESS address = memory_freelist_alloc(page_count);
    if (address == 0) {
        address = memory_pages_alloc_from_regions(page_count);
    }

    if (address == 0) {
        // ERROR HANDLING FIX #3: Log memory exhaustion
        diag_log(2, 0x0201, page_count, g_outstanding_pages);
        return 0;
    }

    if (!memory_freelist_track_active(address, page_count)) {
        if (address != 0) {
            if (!memory_freelist_push(address, page_count)) {
                memory_freelist_merge();
                if (!memory_freelist_push(address, page_count)) {
                }
            }
        }
        return 0;
    }

    g_outstanding_pages += page_count;
    ++g_page_alloc_count;
    return address;
}

BOOLEAN memory_release_pages(EFI_PHYSICAL_ADDRESS address, UINTN page_count) {
    if (address == 0 || page_count == 0) {
        return FALSE;
    }

    if (!memory_freelist_untrack_active(address, page_count)) {
        return FALSE;
    }

    if (!memory_freelist_push(address, page_count)) {
        return FALSE;
    }

    memory_freelist_merge();
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
    return memory_pages_total();
}

UINTN memory_free_pages(void) {
    return memory_pages_free_estimate();
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
