#include "vm.h"

#include "memory.h"
#include "internal/vm_internal.h"

static vm_state_t g_spaces[VM_MAX_SPACES];
static UINTN g_space_count;
static EFI_PHYSICAL_ADDRESS g_kernel_pml4;
static UINTN g_mapped_bytes;
static UINTN g_template_pd_count;
static BOOLEAN g_ready;

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

    return vm_builder_create_identity_space(slot, pd_count);
}

static UINT64 max_u64(UINT64 a, UINT64 b) {
    return (a > b) ? a : b;
}

void vm_init(const platform_context_t *platform) {
    reset_spaces();
    vm_builder_clear_protected_regions();

    if (platform == NULL) {
        return;
    }

    vm_builder_add_protected_region(0x0, 256, TRUE);

    if (platform->framebuffer.base > UINT64_MAX - platform->framebuffer.size) {
        return;
    }

    UINT64 framebuffer_end = platform->framebuffer.base + platform->framebuffer.size;
    UINT64 fb_pages = (platform->framebuffer.size + 4095) / 4096;
    vm_builder_add_protected_region(platform->framebuffer.base, fb_pages, FALSE);

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
