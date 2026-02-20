#include "vm.h"

#include "memory.h"

#define PAGE_TABLE_ENTRIES 512
#define PAGE_2M_SIZE ((UINT64)0x200000)
#define ONE_GIB ((UINT64)0x40000000)
#define FOUR_GIB ((UINT64)0x100000000)
#define VM_MAX_PDPT 8
#define VM_MAX_SPACES 64

#define PAGE_FLAG_PRESENT ((UINT64)0x001)
#define PAGE_FLAG_WRITABLE ((UINT64)0x002)
#define PAGE_FLAG_LARGE ((UINT64)0x080)

typedef struct {
    EFI_PHYSICAL_ADDRESS pml4_phys;
    EFI_PHYSICAL_ADDRESS pdpt_phys;
    EFI_PHYSICAL_ADDRESS pds_phys[VM_MAX_PDPT];
    UINTN pd_count;
    BOOLEAN active;
} vm_state_t;

static vm_state_t g_spaces[VM_MAX_SPACES];
static UINTN g_space_count;
static EFI_PHYSICAL_ADDRESS g_kernel_pml4;
static UINTN g_mapped_bytes;
static UINTN g_template_pd_count;
static BOOLEAN g_ready;

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

static void reset_spaces(void) {
    g_space_count = 0;
    g_kernel_pml4 = 0;
    g_mapped_bytes = 0;
    g_template_pd_count = 0;
    g_ready = FALSE;

    for (UINTN i = 0; i < VM_MAX_SPACES; ++i) {
        g_spaces[i].pml4_phys = 0;
        g_spaces[i].pdpt_phys = 0;
        g_spaces[i].pd_count = 0;
        g_spaces[i].active = FALSE;
        for (UINTN j = 0; j < VM_MAX_PDPT; ++j) {
            g_spaces[i].pds_phys[j] = 0;
        }
    }
}

static INTN find_space(EFI_PHYSICAL_ADDRESS root) {
    for (UINTN i = 0; i < g_space_count; ++i) {
        if (g_spaces[i].active && g_spaces[i].pml4_phys == root) {
            return (INTN)i;
        }
    }
    return -1;
}

static vm_state_t *alloc_space_slot(void) {
    for (UINTN i = 0; i < g_space_count; ++i) {
        if (!g_spaces[i].active) {
            return &g_spaces[i];
        }
    }

    if (g_space_count >= VM_MAX_SPACES) {
        return NULL;
    }

    return &g_spaces[g_space_count++];
}

static EFI_PHYSICAL_ADDRESS create_identity_space(UINTN pd_count) {
    vm_state_t *slot = alloc_space_slot();
    if (slot == NULL) {
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

    for (UINTN pdpt_index = 0; pdpt_index < pd_count; ++pdpt_index) {
        EFI_PHYSICAL_ADDRESS pd_phys = 0;
        UINT64 *pd = alloc_table_page(&pd_phys);
        if (pd == NULL) {
            (void)vm_release_address_space(slot->pml4_phys);
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

static UINT64 max_u64(UINT64 a, UINT64 b) {
    return (a > b) ? a : b;
}

void vm_init(const platform_context_t *platform) {
    reset_spaces();

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

    g_template_pd_count = pd_count;
    g_mapped_bytes = (UINTN)(pd_count * ONE_GIB);
    g_kernel_pml4 = create_identity_space(pd_count);
    g_ready = (g_kernel_pml4 != 0);
}

BOOLEAN vm_is_ready(void) {
    return g_ready;
}

EFI_PHYSICAL_ADDRESS vm_pml4_physical(void) {
    return g_kernel_pml4;
}

UINTN vm_mapped_bytes(void) {
    return g_mapped_bytes;
}

EFI_PHYSICAL_ADDRESS vm_create_address_space(void) {
    if (!g_ready) {
        return 0;
    }

    return create_identity_space(g_template_pd_count);
}

BOOLEAN vm_release_address_space(EFI_PHYSICAL_ADDRESS root) {
    if (root == 0 || root == g_kernel_pml4) {
        return FALSE;
    }

    INTN slot_index = find_space(root);
    if (slot_index < 0) {
        return FALSE;
    }

    vm_state_t *slot = &g_spaces[(UINTN)slot_index];
    for (UINTN i = 0; i < slot->pd_count; ++i) {
        if (slot->pds_phys[i] != 0) {
            (void)memory_release_pages(slot->pds_phys[i], 1);
            slot->pds_phys[i] = 0;
        }
    }

    if (slot->pdpt_phys != 0) {
        (void)memory_release_pages(slot->pdpt_phys, 1);
    }
    if (slot->pml4_phys != 0) {
        (void)memory_release_pages(slot->pml4_phys, 1);
    }

    slot->active = FALSE;
    slot->pml4_phys = 0;
    slot->pdpt_phys = 0;
    slot->pd_count = 0;
    return TRUE;
}
