#ifndef EXIT_BOOT_SERVICES_H
#define EXIT_BOOT_SERVICES_H

#include "boot_info.h"
#include "uefi.h"

// Call from efi_main *after* every protocol the kernel needs has been
// located and every value it cares about has been copied into boot_info.
// Returns EFI_SUCCESS on a clean exit; on success the caller must not touch
// system_table->BootServices nor boot_info->bootstrap.boot_services again.
EFI_STATUS exit_boot_services(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *system_table, boot_info_t *boot_info);

#endif
