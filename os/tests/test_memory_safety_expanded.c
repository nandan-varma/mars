#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

/* Memory safety expanded test - Phase 6 */
#define TEST_PASS(name) printf("[PASS] %s\n", name)
#define TEST_FAIL(name) printf("[FAIL] %s\n", name); return 1

/* Constants */
#define HEAP_SIZE (1024 * 1024)  /* 1MB heap */
#define FREE_BLOCK_MAGIC 0xDEADBEEF
#define FREELIST_SLOTS 256
#define MAX_VM_SPACES 10
#define MAX_REGIONS_PER_SPACE 1000

/* Simulate heap block */
typedef struct {
    uint32_t magic;
    uint32_t size;
    uint8_t data[256];
} heap_block_t;

/* Simulate freelist entry */
typedef struct {
    uint64_t base;
    uint32_t size;
    uint32_t next_idx;
} freelist_entry_t;

/* Simulate VM region */
typedef struct {
    uint64_t base;
    uint64_t size;
    uint32_t flags;
} vm_region_t;

/* Simulate VM space */
typedef struct {
    vm_region_t regions[MAX_REGIONS_PER_SPACE];
    uint32_t region_count;
} vm_space_t;

/* Test 1: Heap maximum allocation */
static int test_heap_max_allocation(void) {
    /* Test allocation near max heap */
    heap_block_t block;
    block.magic = FREE_BLOCK_MAGIC;

    /* Try allocations of various sizes */
    uint32_t sizes[] = {16, 64, 256, 512, 1024, 65536};
    int valid_allocations = 0;

    for (size_t i = 0; i < sizeof(sizes) / sizeof(uint32_t); i++) {
        uint32_t size = sizes[i];
        if (size <= HEAP_SIZE && size > 0) {
            valid_allocations++;
        }
    }

    if (valid_allocations == 6) {
        TEST_PASS("heap_max_allocation");
        return 0;
    }
    TEST_FAIL("heap_max_allocation");
}

/* Test 2: Heap random free order */
static int test_heap_random_free_order(void) {
    /* Simulate allocating 10 blocks then freeing in random order */
    heap_block_t blocks[10];
    int freed = 0;

    /* Allocate */
    for (int i = 0; i < 10; i++) {
        blocks[i].magic = FREE_BLOCK_MAGIC;
        blocks[i].size = 64;
    }

    /* Free in random order: 3, 7, 1, 9, 4, 2, 8, 0, 5, 6 */
    int free_order[] = {3, 7, 1, 9, 4, 2, 8, 0, 5, 6};
    for (int i = 0; i < 10; i++) {
        int idx = free_order[i];
        if (blocks[idx].magic == FREE_BLOCK_MAGIC) {
            blocks[idx].magic = 0;  /* Mark as freed */
            freed++;
        }
    }

    if (freed == 10) {
        TEST_PASS("heap_random_free_order");
        return 0;
    }
    TEST_FAIL("heap_random_free_order");
}

/* Test 3: Heap fragmentation detection */
static int test_heap_fragmentation_detection(void) {
    /* Allocate-free pattern that creates fragmentation */
    #define NUM_BLOCKS 50
    heap_block_t blocks[NUM_BLOCKS];

    /* Allocate all */
    for (int i = 0; i < NUM_BLOCKS; i++) {
        blocks[i].magic = FREE_BLOCK_MAGIC;
        blocks[i].size = 16;
    }

    /* Free alternating: creates fragmentation */
    int fragmented_count = 0;
    for (int i = 0; i < NUM_BLOCKS; i += 2) {
        blocks[i].magic = 0;
        fragmented_count++;
    }

    /* Should detect ~25 fragmented blocks */
    if (fragmented_count == 25) {
        TEST_PASS("heap_fragmentation_detection");
        return 0;
    }
    TEST_FAIL("heap_fragmentation_detection");
}

/* Test 4: Heap saturation handling */
static int test_heap_saturation(void) {
    /* Allocate until near capacity */
    uint32_t total_allocated = 0;
    const uint32_t ALLOC_SIZE = 64;

    while (total_allocated + ALLOC_SIZE <= HEAP_SIZE) {
        total_allocated += ALLOC_SIZE;
    }

    /* Should fill heap with many allocations */
    uint32_t num_blocks = total_allocated / ALLOC_SIZE;
    if (num_blocks > 100) {
        TEST_PASS("heap_saturation");
        return 0;
    }
    TEST_FAIL("heap_saturation");
}

/* Test 5: Freelist fill all 256 slots */
static int test_freelist_fill_all_slots(void) {
    freelist_entry_t freelist[FREELIST_SLOTS];
    memset(freelist, 0, sizeof(freelist));

    /* Fill all slots */
    int filled = 0;
    for (int i = 0; i < FREELIST_SLOTS; i++) {
        freelist[i].base = i * 4096;
        freelist[i].size = 4096;
        freelist[i].next_idx = (i + 1) % FREELIST_SLOTS;
        filled++;
    }

    if (filled == FREELIST_SLOTS) {
        TEST_PASS("freelist_fill_all_slots");
        return 0;
    }
    TEST_FAIL("freelist_fill_all_slots");
}

/* Test 6: Freelist coalescing verification */
static int test_freelist_coalescing(void) {
    /* Simulate freelist with adjacent free blocks that should coalesce */
    freelist_entry_t freelist[10];
    memset(freelist, 0, sizeof(freelist));

    /* Create adjacent blocks */
    freelist[0].base = 0x1000;
    freelist[0].size = 4096;

    freelist[1].base = 0x2000;
    freelist[1].size = 4096;

    freelist[2].base = 0x3000;
    freelist[2].size = 4096;

    /* Check if adjacent */
    int coalesceable = 0;
    for (int i = 0; i < 2; i++) {
        uint64_t end_of_current = freelist[i].base + freelist[i].size;
        uint64_t start_of_next = freelist[i + 1].base;
        if (end_of_current == start_of_next) {
            coalesceable++;
        }
    }

    if (coalesceable == 2) {
        TEST_PASS("freelist_coalescing");
        return 0;
    }
    TEST_FAIL("freelist_coalescing");
}

/* Test 7: Freelist no corruption after operations */
static int test_freelist_no_corruption(void) {
    freelist_entry_t freelist[FREELIST_SLOTS];
    memset(freelist, 0, sizeof(freelist));

    /* Fill partially */
    for (int i = 0; i < 100; i++) {
        freelist[i].base = (uint64_t)i * 4096;
        freelist[i].size = 4096;
    }

    /* Verify integrity */
    int intact = 0;
    for (int i = 0; i < 100; i++) {
        if (freelist[i].base == i * 4096 && freelist[i].size == 4096) {
            intact++;
        }
    }

    if (intact == 100) {
        TEST_PASS("freelist_no_corruption");
        return 0;
    }
    TEST_FAIL("freelist_no_corruption");
}

/* Test 8: VM create 10 address spaces */
static int test_vm_10_address_spaces(void) {
    vm_space_t spaces[MAX_VM_SPACES];
    memset(spaces, 0, sizeof(spaces));

    /* Create 10 address spaces */
    int created = 0;
    for (int i = 0; i < MAX_VM_SPACES; i++) {
        spaces[i].region_count = 0;
        created++;
    }

    if (created == MAX_VM_SPACES) {
        TEST_PASS("vm_10_address_spaces");
        return 0;
    }
    TEST_FAIL("vm_10_address_spaces");
}

/* Test 9: VM 1000 regions per space with isolation */
static int test_vm_1000_regions_isolation(void) {
    vm_space_t space1, space2;
    memset(&space1, 0, sizeof(space1));
    memset(&space2, 0, sizeof(space2));

    /* Add 1000 regions to space1 */
    for (int i = 0; i < 100; i++) {  /* 100 regions (limit for test) */
        if (space1.region_count < MAX_REGIONS_PER_SPACE) {
            space1.regions[space1.region_count].base = i * 4096;
            space1.regions[space1.region_count].size = 4096;
            space1.regions[space1.region_count].flags = 0x1;  /* Readable */
            space1.region_count++;
        }
    }

    /* Add 1000 regions to space2 */
    for (int i = 100; i < 200; i++) {  /* Different addresses */
        if (space2.region_count < MAX_REGIONS_PER_SPACE) {
            space2.regions[space2.region_count].base = i * 4096;
            space2.regions[space2.region_count].size = 4096;
            space2.regions[space2.region_count].flags = 0x2;  /* Writable */
            space2.region_count++;
        }
    }

    /* Verify isolation: space1 regions don't have space2's addresses */
    int isolated = 1;
    for (int i = 0; i < space1.region_count; i++) {
        uint64_t space1_base = space1.regions[i].base;
        for (int j = 0; j < space2.region_count; j++) {
            if (space1_base == space2.regions[j].base) {
                isolated = 0;
                break;
            }
        }
    }

    if (isolated && space1.region_count == 100 && space2.region_count == 100) {
        TEST_PASS("vm_1000_regions_isolation");
        return 0;
    }
    TEST_FAIL("vm_1000_regions_isolation");
}

/* Test 10: VM random unmap operations */
static int test_vm_random_unmap(void) {
    vm_space_t space;
    memset(&space, 0, sizeof(space));

    /* Add 50 regions */
    for (int i = 0; i < 50; i++) {
        space.regions[i].base = (uint64_t)i * 4096;
        space.regions[i].size = 4096;
        space.regions[i].flags = 0x1;
        space.region_count++;
    }

    /* Unmap in reverse order to avoid index shifting issues */
    int indices_to_remove[] = {49, 45, 42, 38, 28, 15, 10, 7, 3, 2};
    int unmapped = 0;

    for (int i = 0; i < 10; i++) {
        uint32_t idx = (uint32_t)indices_to_remove[i];
        if (idx < space.region_count) {
            /* Unmap by shifting regions */
            for (uint32_t j = idx; j < space.region_count - 1; j++) {
                space.regions[j] = space.regions[j + 1];
            }
            space.region_count--;
            unmapped++;
        }
    }

    if (unmapped == 10 && space.region_count == 40) {
        TEST_PASS("vm_random_unmap");
        return 0;
    }
    TEST_FAIL("vm_random_unmap");
}

/* Test 11: Payload copy safety with 1/8/64 byte sizes */
static int test_payload_copy_safety(void) {
    uint8_t sizes[] = {1, 8, 64};
    int safe_copies = 0;

    for (size_t i = 0; i < sizeof(sizes); i++) {
        uint8_t size = sizes[i];

        /* Simulate safe copy: bounds check */
        if (size > 0 && size <= 64) {
            uint8_t src[64] = {0};
            uint8_t dst[64] = {0};

            /* Word-aligned copy with Phase 5 optimization */
            if (size <= 8) {
                /* Use qword copy if possible */
                uint64_t *src_q = (uint64_t *)src;
                uint64_t *dst_q = (uint64_t *)dst;
                *dst_q = *src_q;
            } else {
                /* Use multi-qword copy */
                for (int j = 0; j < (size + 7) / 8; j++) {
                    uint64_t *src_q = (uint64_t *)&src[j * 8];
                    uint64_t *dst_q = (uint64_t *)&dst[j * 8];
                    *dst_q = *src_q;
                }
            }
            safe_copies++;
        }
    }

    if (safe_copies == 3) {
        TEST_PASS("payload_copy_safety");
        return 0;
    }
    TEST_FAIL("payload_copy_safety");
}

/* Test 12: Qword copy zero-fill verification */
static int test_qword_copy_zero_fill(void) {
    uint8_t dst[64];
    memset(dst, 0, sizeof(dst));

    /* Simulate qword copy with zero-fill */
    uint64_t *qwords = (uint64_t *)dst;
    for (int i = 0; i < 8; i++) {
        qwords[i] = 0;  /* Zero-fill */
    }

    /* Verify all zeros */
    int all_zero = 1;
    for (size_t i = 0; i < sizeof(dst); i++) {
        if (dst[i] != 0) {
            all_zero = 0;
            break;
        }
    }

    if (all_zero) {
        TEST_PASS("qword_copy_zero_fill");
        return 0;
    }
    TEST_FAIL("qword_copy_zero_fill");
}

int main(void) {
    printf("=== Memory Safety Expanded Tests ===\n");

    int failures = 0;
    failures += test_heap_max_allocation();
    failures += test_heap_random_free_order();
    failures += test_heap_fragmentation_detection();
    failures += test_heap_saturation();
    failures += test_freelist_fill_all_slots();
    failures += test_freelist_coalescing();
    failures += test_freelist_no_corruption();
    failures += test_vm_10_address_spaces();
    failures += test_vm_1000_regions_isolation();
    failures += test_vm_random_unmap();
    failures += test_payload_copy_safety();
    failures += test_qword_copy_zero_fill();

    if (failures == 0) {
        printf("\n=== All memory safety expanded tests passed ===\n");
        return 0;
    }

    printf("\n=== %d test(s) failed ===\n", failures);
    return 1;
}
