#include "vm_internal.h"

#include "memory.h"

#define PAGE_FLAG_PRESENT ((UINT64)0x001)
#define PAGE_FLAG_WRITABLE ((UINT64)0x002)
#define PAGE_FLAG_LARGE ((UINT64)0x080)

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

EFI_PHYSICAL_ADDRESS vm_builder_create_identity_space(vm_state_t *slot, UINTN pd_count) {
    if (slot == NULL || pd_count == 0 || pd_count > VM_MAX_PDPT) {
        return 0;
    }

    EFI_PHYSICAL_ADDRESS pml4_phys = 0;
    UINT64 *pml4 = alloc_table_page(&pml4_phys);
    if (pml4 == NULL) {
        return 0;
    }

    EFI_PHYSICAL_ADDRESS pdpt_phys = 0;
    UINT64 *pdpt = alloc_table_page(&pdpt_phys);
    if (pdpt == NULL) {
        (void)memory_release_pages(pml4_phys, 1);
        return 0;
    }

    pml4[0] = ((UINT64)pdpt_phys) | PAGE_FLAG_PRESENT | PAGE_FLAG_WRITABLE;

    slot->pml4_phys = pml4_phys;
    slot->pdpt_phys = pdpt_phys;
    slot->pd_count = pd_count;
    slot->active = TRUE;
    for (UINTN i = 0; i < VM_MAX_PDPT; ++i) {
        slot->pds_phys[i] = 0;
    }

    for (UINTN pdpt_index = 0; pdpt_index < pd_count; ++pdpt_index) {
        EFI_PHYSICAL_ADDRESS pd_phys = 0;
        UINT64 *pd = alloc_table_page(&pd_phys);
        if (pd == NULL) {
            for (UINTN j = 0; j < pdpt_index; ++j) {
                if (slot->pds_phys[j] != 0) {
                    (void)memory_release_pages(slot->pds_phys[j], 1);
                    slot->pds_phys[j] = 0;
                }
            }
            (void)memory_release_pages(pdpt_phys, 1);
            (void)memory_release_pages(pml4_phys, 1);
            slot->active = FALSE;
            slot->pml4_phys = 0;
            slot->pdpt_phys = 0;
            slot->pd_count = 0;
            return 0;
        }

        slot->pds_phys[pdpt_index] = pd_phys;
        pdpt[pdpt_index] = ((UINT64)pd_phys) | PAGE_FLAG_PRESENT | PAGE_FLAG_WRITABLE;

        UINT64 base = ((UINT64)pdpt_index) * ONE_GIB;
        for (UINTN pde_index = 0; pde_index < PAGE_TABLE_ENTRIES; ++pde_index) {
            UINT64 physical = base + ((UINT64)pde_index * PAGE_2M_SIZE);
            pd[pde_index] = physical | PAGE_FLAG_PRESENT | PAGE_FLAG_WRITABLE | PAGE_FLAG_LARGE;
        }
    }

    return slot->pml4_phys;
}
