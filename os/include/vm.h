#ifndef VM_H
#define VM_H

#include "platform.h"

void vm_init(const platform_context_t *platform);
BOOLEAN vm_is_ready(void);
EFI_PHYSICAL_ADDRESS vm_pml4_physical(void);
UINTN vm_mapped_bytes(void);
EFI_PHYSICAL_ADDRESS vm_create_address_space(void);
BOOLEAN vm_release_address_space(EFI_PHYSICAL_ADDRESS root);

#endif
