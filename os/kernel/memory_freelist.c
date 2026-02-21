#include "memory_internal.h"

#define MAX_PAGE_ALLOCS 256
#define MAX_FREE_BLOCKS 256

typedef struct {
    EFI_PHYSICAL_ADDRESS base;
    UINTN pages;
    BOOLEAN active;
} page_block_t;

static page_block_t g_active_allocs[MAX_PAGE_ALLOCS];
static page_block_t g_free_blocks[MAX_FREE_BLOCKS];

static void clear_blocks(page_block_t *blocks, UINTN count) {
    for (UINTN i = 0; i < count; ++i) {
        blocks[i].base = 0;
        blocks[i].pages = 0;
        blocks[i].active = FALSE;
    }
}

void memory_freelist_reset(void) {
    clear_blocks(g_active_allocs, MAX_PAGE_ALLOCS);
    clear_blocks(g_free_blocks, MAX_FREE_BLOCKS);
}

BOOLEAN memory_freelist_track_active(EFI_PHYSICAL_ADDRESS base, UINTN pages) {
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

BOOLEAN memory_freelist_untrack_active(EFI_PHYSICAL_ADDRESS base, UINTN pages) {
    for (UINTN i = 0; i < MAX_PAGE_ALLOCS; ++i) {
        if (!g_active_allocs[i].active) {
            continue;
        }

        if (g_active_allocs[i].base == base && g_active_allocs[i].pages == pages) {
            g_active_allocs[i].active = FALSE;
            return TRUE;
        }
    }

    return FALSE;
}

BOOLEAN memory_freelist_push(EFI_PHYSICAL_ADDRESS base, UINTN pages) {
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

void memory_freelist_merge(void) {
    for (UINTN i = 0; i < MAX_FREE_BLOCKS; ++i) {
        if (!g_free_blocks[i].active) {
            continue;
        }

        EFI_PHYSICAL_ADDRESS i_end = g_free_blocks[i].base + (EFI_PHYSICAL_ADDRESS)((UINT64)g_free_blocks[i].pages * PAGE_SIZE);

        for (UINTN j = 0; j < MAX_FREE_BLOCKS; ++j) {
            if (i == j || !g_free_blocks[j].active) {
                continue;
            }

            EFI_PHYSICAL_ADDRESS j_end = g_free_blocks[j].base + (EFI_PHYSICAL_ADDRESS)((UINT64)g_free_blocks[j].pages * PAGE_SIZE);
            if (i_end == g_free_blocks[j].base) {
                g_free_blocks[i].pages += g_free_blocks[j].pages;
                g_free_blocks[j].active = FALSE;
                i_end = g_free_blocks[i].base + (EFI_PHYSICAL_ADDRESS)((UINT64)g_free_blocks[i].pages * PAGE_SIZE);
            } else if (j_end == g_free_blocks[i].base) {
                g_free_blocks[i].base = g_free_blocks[j].base;
                g_free_blocks[i].pages += g_free_blocks[j].pages;
                g_free_blocks[j].active = FALSE;
                i_end = g_free_blocks[i].base + (EFI_PHYSICAL_ADDRESS)((UINT64)g_free_blocks[i].pages * PAGE_SIZE);
            }
        }
    }
}

EFI_PHYSICAL_ADDRESS memory_freelist_alloc(UINTN page_count) {
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
