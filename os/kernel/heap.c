#include "heap.h"

#include "memory.h"
#include "spinlock.h"

#define HEAP_ALIGNMENT 16
#define PAGE_SIZE 4096
#define MAX_FREE_BLOCKS 128

typedef struct free_block {
    UINTN size;
    struct free_block *next;
} free_block_t;

static UINT8 *g_heap_base;
static UINTN g_heap_total;
static UINTN g_heap_used;
static free_block_t *g_free_list;
static spinlock_t g_heap_lock;

static UINTN align_up(UINTN value, UINTN align) {
    if (align == 0) {
        return value;
    }
    return (value + align - 1) & ~(align - 1);
}

void heap_init(UINTN initial_pages) {
    spinlock_init(&g_heap_lock);

    g_heap_base = NULL;
    g_heap_total = 0;
    g_heap_used = 0;
    g_free_list = NULL;

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
    g_heap_used = 0;

    free_block_t *initial_block = (free_block_t *)g_heap_base;
    initial_block->size = total - sizeof(free_block_t);
    initial_block->next = NULL;
    g_free_list = initial_block;
}

void *heap_alloc(UINTN size) {
    if (size == 0 || g_heap_base == NULL) {
        return NULL;
    }

    size = align_up(size, HEAP_ALIGNMENT);

    spinlock_acquire(&g_heap_lock);

    free_block_t **prev = &g_free_list;
    free_block_t *block = g_free_list;

    while (block != NULL) {
        if (block->size >= size) {
            UINTN remaining = block->size - size;

            if (remaining >= sizeof(free_block_t) + HEAP_ALIGNMENT) {
                free_block_t *new_block = (free_block_t *)((UINT8 *)block + sizeof(free_block_t) + size);
                new_block->size = remaining - sizeof(free_block_t);
                new_block->next = block->next;
                *prev = new_block;
            } else {
                *prev = block->next;
            }

            g_heap_used += size;
            spinlock_release(&g_heap_lock);
            return (UINT8 *)block + sizeof(free_block_t);
        }

        prev = &block->next;
        block = block->next;
    }

    UINTN needed = align_up(size + sizeof(free_block_t), PAGE_SIZE);
    UINTN current_aligned = align_up(g_heap_used + sizeof(free_block_t), PAGE_SIZE);

    if (current_aligned + needed > g_heap_total) {
        spinlock_release(&g_heap_lock);
        return NULL;
    }

    free_block_t *new_block = (free_block_t *)(g_heap_base + current_aligned);
    new_block->size = needed - sizeof(free_block_t);
    new_block->next = g_free_list;
    g_free_list = new_block;

    g_heap_used = current_aligned + needed;
    spinlock_release(&g_heap_lock);

    return (UINT8 *)new_block + sizeof(free_block_t);
}

void heap_free(void *ptr) {
    if (ptr == NULL || g_heap_base == NULL) {
        return;
    }

    UINT8 *ptr_byte = (UINT8 *)ptr;
    if (ptr_byte < g_heap_base || ptr_byte >= g_heap_base + g_heap_total) {
        return;
    }

    free_block_t *block = (free_block_t *)(ptr_byte - sizeof(free_block_t));

    spinlock_acquire(&g_heap_lock);

    free_block_t **prev = &g_free_list;
    free_block_t *curr = g_free_list;

    while (curr != NULL) {
        if (block < curr) {
            block->next = curr;
            *prev = block;
            break;
        }
        prev = &curr->next;
        curr = curr->next;
    }

    if (curr == NULL) {
        block->next = NULL;
        *prev = block;
    }

    free_block_t *merged = g_free_list;
    while (merged != NULL && merged->next != NULL) {
        UINT8 *block_end = (UINT8 *)merged + sizeof(free_block_t) + merged->size;
        if (block_end == (UINT8 *)merged->next) {
            merged->size += sizeof(free_block_t) + merged->next->size;
            merged->next = merged->next->next;
        } else {
            merged = merged->next;
        }
    }

    spinlock_release(&g_heap_lock);
}

UINTN heap_total_bytes(void) {
    return g_heap_total;
}

UINTN heap_used_bytes(void) {
    spinlock_acquire(&g_heap_lock);
    UINTN used = g_heap_used;

    free_block_t *block = g_free_list;
    while (block != NULL) {
        used -= (block->size + sizeof(free_block_t));
        block = block->next;
    }

    spinlock_release(&g_heap_lock);
    return used;
}
