#ifndef VM_INTERNAL_H
#define VM_INTERNAL_H

#include "vm.h"

#define PAGE_TABLE_ENTRIES 512
#define PAGE_2M_SIZE ((UINT64)0x200000)
#define ONE_GIB ((UINT64)0x40000000)
#define FOUR_GIB ((UINT64)0x100000000)
#define VM_MAX_PDPT 8
#define VM_MAX_SPACES 64

typedef struct {
    EFI_PHYSICAL_ADDRESS pml4_phys;
    EFI_PHYSICAL_ADDRESS pdpt_phys;
    EFI_PHYSICAL_ADDRESS pds_phys[VM_MAX_PDPT];
    UINTN pd_count;
    BOOLEAN active;
} vm_state_t;

void vm_builder_add_protected_region(EFI_PHYSICAL_ADDRESS base, UINTN pages, BOOLEAN read_only);
void vm_builder_clear_protected_regions(void);
EFI_PHYSICAL_ADDRESS vm_builder_create_identity_space(vm_state_t *slot, UINTN pd_count);

#endif
