#include "vm.h"

#include "memory.h"

#define PAGE_TABLE_ENTRIES 512
#define PAGE_2M_SIZE ((UINT64)0x200000)
#define ONE_GIB ((UINT64)0x40000000)
#define FOUR_GIB ((UINT64)0x100000000)
#define VM_MAX_PDPT 8

#define PAGE_FLAG_PRESENT ((UINT64)0x001)
#define PAGE_FLAG_WRITABLE ((UINT64)0x002)
#define PAGE_FLAG_LARGE ((UINT64)0x080)

typedef struct {
    UINT64 *pml4;
    UINT64 *pdpt;
    UINT64 *pds[VM_MAX_PDPT];
    UINTN pd_count;
    EFI_PHYSICAL_ADDRESS pml4_phys;
    UINTN mapped_bytes;
    BOOLEAN ready;
} vm_state_t;

static vm_state_t g_vm;

static void zero_page(UINT64 *page) {
    if (page == NULL) {
        return;
    }

    for (UINTN i = 0; i < PAGE_TABLE_ENTRIES; ++i) {
        page[i] = 0;
    }
}

static UINT64 *alloc_table_page(EFI_PHYSICAL_ADDRESS *physical_out) {
    EFI_PHYSICAL_ADDRESS physical = memory_alloc_pages(1);
    if (physical == 0) {
        return NULL;
    }

    UINT64 *table = (UINT64 *)(UINTN)physical;
    zero_page(table);

    if (physical_out != NULL) {
        *physical_out = physical;
    }

    return table;
}

static UINT64 max_u64(UINT64 a, UINT64 b) {
    return (a > b) ? a : b;
}

void vm_init(const platform_context_t *platform) {
    g_vm.pml4 = NULL;
    g_vm.pdpt = NULL;
    g_vm.pml4_phys = 0;
    g_vm.mapped_bytes = 0;
    g_vm.pd_count = 0;
    g_vm.ready = FALSE;

    for (UINTN i = 0; i < VM_MAX_PDPT; ++i) {
        g_vm.pds[i] = NULL;
    }

    if (platform == NULL) {
        return;
    }

    UINT64 framebuffer_end = (UINT64)platform->framebuffer.base + (UINT64)platform->framebuffer.size;
    UINT64 requested_span = max_u64(FOUR_GIB, framebuffer_end);

    UINT64 max_span = (UINT64)VM_MAX_PDPT * ONE_GIB;
    UINT64 map_span = requested_span;
    if (map_span > max_span) {
        map_span = max_span;
    }

    UINTN pd_count = (UINTN)((map_span + ONE_GIB - 1) / ONE_GIB);
    if (pd_count == 0 || pd_count > VM_MAX_PDPT) {
        return;
    }

    EFI_PHYSICAL_ADDRESS pml4_phys = 0;
    g_vm.pml4 = alloc_table_page(&pml4_phys);
    if (g_vm.pml4 == NULL) {
        return;
    }

    EFI_PHYSICAL_ADDRESS pdpt_phys = 0;
    g_vm.pdpt = alloc_table_page(&pdpt_phys);
    if (g_vm.pdpt == NULL) {
        return;
    }

    g_vm.pml4[0] = ((UINT64)pdpt_phys) | PAGE_FLAG_PRESENT | PAGE_FLAG_WRITABLE;

    for (UINTN pdpt_index = 0; pdpt_index < pd_count; ++pdpt_index) {
        EFI_PHYSICAL_ADDRESS pd_phys = 0;
        UINT64 *pd = alloc_table_page(&pd_phys);
        if (pd == NULL) {
            return;
        }

        g_vm.pds[pdpt_index] = pd;
        g_vm.pdpt[pdpt_index] = ((UINT64)pd_phys) | PAGE_FLAG_PRESENT | PAGE_FLAG_WRITABLE;

        UINT64 base = ((UINT64)pdpt_index) * ONE_GIB;
        for (UINTN pde_index = 0; pde_index < PAGE_TABLE_ENTRIES; ++pde_index) {
            UINT64 physical = base + ((UINT64)pde_index * PAGE_2M_SIZE);
            pd[pde_index] = physical | PAGE_FLAG_PRESENT | PAGE_FLAG_WRITABLE | PAGE_FLAG_LARGE;
        }
    }

    g_vm.pml4_phys = pml4_phys;
    g_vm.pd_count = pd_count;
    g_vm.mapped_bytes = (UINTN)(pd_count * ONE_GIB);
    g_vm.ready = TRUE;
}

BOOLEAN vm_is_ready(void) {
    return g_vm.ready;
}

EFI_PHYSICAL_ADDRESS vm_pml4_physical(void) {
    return g_vm.pml4_phys;
}

UINTN vm_mapped_bytes(void) {
    return g_vm.mapped_bytes;
}
