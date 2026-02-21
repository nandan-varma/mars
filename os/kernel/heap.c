#include "heap.h"

#include "memory.h"

#define HEAP_ALIGNMENT 16
#define PAGE_SIZE 4096

static UINT8 *g_heap_base;
static UINTN g_heap_total;
static UINTN g_heap_used;

static UINTN align_up(UINTN value, UINTN align) {
    if (align == 0) {
        return value;
    }
    return (value + align - 1) & ~(align - 1);
}

void heap_init(UINTN initial_pages) {
    g_heap_base = NULL;
    g_heap_total = 0;
    g_heap_used = 0;

    if (initial_pages == 0) {
        return;
    }

    EFI_PHYSICAL_ADDRESS region = memory_alloc_pages(initial_pages);
    if (region == 0) {
        return;
    }

    UINTN total = initial_pages * PAGE_SIZE;
    if (total / PAGE_SIZE != initial_pages) {
        (void)memory_release_pages(region, initial_pages);
        return;
    }

    g_heap_base = (UINT8 *)(UINTN)region;
    g_heap_total = total;
}

void *heap_alloc(UINTN size) {
    if (size == 0 || g_heap_base == NULL) {
        return NULL;
    }

    UINTN offset = align_up(g_heap_used, HEAP_ALIGNMENT);
    if (offset > g_heap_total || size > g_heap_total - offset) {
        return NULL;
    }

    void *ptr = g_heap_base + offset;
    g_heap_used = offset + size;
    return ptr;
}

UINTN heap_total_bytes(void) {
    return g_heap_total;
}

UINTN heap_used_bytes(void) {
    return g_heap_used;
}