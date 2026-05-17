#include "boot_info.h"
#include "exit_boot_services.h"
#include "serial.h"
#include "uefi.h"

// Cleanly exit UEFI Boot Services and pin the memory map.
//
// The protocol is: GetMemoryMap → ExitBootServices(map_key). The map can
// grow between the two calls because firmware may itself allocate as part
// of teardown, so we retry up to EXIT_BOOT_SERVICES_MAX_RETRIES times. We
// also allocate the map buffer with a generous slack so the first try
// usually fits even if firmware reorganizes a few descriptors.
//
// After this returns EFI_SUCCESS the caller MUST treat
// system_table->BootServices and boot_info->bootstrap.boot_services as
// poisoned — calls into them are undefined behavior.
//
// What is captured before the exit (and continues to work after):
//   - boot_info->framebuffer (base, size, width, height, pitch) — already
//     populated in efi_main from gop->Mode before this is called.
//   - boot_info->input.* — protocol function pointers stay valid in OVMF
//     and most production firmware. Failure mode is firmware-specific.
//   - boot_info->runtime.runtime_services — Runtime Services survive
//     ExitBootServices by spec.
//
// What stops working:
//   - boot_services->AllocatePool / FreePool — use the page allocator.
//   - boot_services->LocateProtocol / HandleProtocol — must have run pre-exit.
//   - boot_services->Stall — use TSC or timer_poll-driven sleeps.

#ifndef EXIT_BOOT_SERVICES_MAX_RETRIES
#define EXIT_BOOT_SERVICES_MAX_RETRIES 8
#endif

// Slack added to the required map size, in descriptors. Picked high enough
// to absorb the memory-map churn that firmware does during ExitBootServices
// itself (typically 1–3 descriptors on OVMF, more on some vendor BIOSes).
#ifndef EXIT_BOOT_SERVICES_SLACK_DESCRIPTORS
#define EXIT_BOOT_SERVICES_SLACK_DESCRIPTORS 32
#endif

EFI_STATUS exit_boot_services(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *system_table, boot_info_t *boot_info) {
    if (image_handle == NULL || system_table == NULL || boot_info == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    EFI_BOOT_SERVICES *bs = system_table->BootServices;
    if (bs == NULL) {
        serial_write_str("[mars] exit_bs: bs == NULL (already exited?)\r\n");
        return EFI_NOT_READY;
    }

    // Probe for the current map size + descriptor metadata.
    UINTN required_size = 0;
    UINTN map_key = 0;
    UINTN desc_size = 0;
    UINT32 desc_version = 0;
    EFI_MEMORY_DESCRIPTOR *probe = NULL;

    EFI_STATUS status = bs->GetMemoryMap(&required_size, probe, &map_key, &desc_size, &desc_version);
    if (status != EFI_BUFFER_TOO_SMALL) {
        serial_write_str("[mars] exit_bs: probe failed ");
        serial_write_hex64((UINT64)status);
        serial_write_str("\r\n");
        return status;
    }

    UINTN map_size = required_size + (desc_size * EXIT_BOOT_SERVICES_SLACK_DESCRIPTORS);
    EFI_MEMORY_DESCRIPTOR *map = NULL;
    status = bs->AllocatePool(EfiLoaderData, map_size, (void **)&map);
    if (EFI_ERROR(status) || map == NULL) {
        serial_write_str("[mars] exit_bs: allocate failed\r\n");
        return status;
    }

    for (UINTN attempt = 0; attempt < EXIT_BOOT_SERVICES_MAX_RETRIES; ++attempt) {
        UINTN size_inout = map_size;
        status = bs->GetMemoryMap(&size_inout, map, &map_key, &desc_size, &desc_version);

        if (status == EFI_BUFFER_TOO_SMALL) {
            // The map grew past our slack between attempts. Free, grow,
            // and retry — but only if we still have budget.
            (void)bs->FreePool(map);
            map_size = size_inout + (desc_size * EXIT_BOOT_SERVICES_SLACK_DESCRIPTORS);
            status = bs->AllocatePool(EfiLoaderData, map_size, (void **)&map);
            if (EFI_ERROR(status) || map == NULL) {
                serial_write_str("[mars] exit_bs: realloc failed\r\n");
                return status;
            }
            continue;
        }
        if (EFI_ERROR(status)) {
            serial_write_str("[mars] exit_bs: get_map failed ");
            serial_write_hex64((UINT64)status);
            serial_write_str("\r\n");
            (void)bs->FreePool(map);
            return status;
        }

        status = bs->ExitBootServices(image_handle, map_key);
        if (status == EFI_SUCCESS) {
            // Update boot_info to point at the post-exit map. We deliberately
            // keep the buffer allocated — AllocatePool memory becomes part
            // of the kernel-owned heap region after exit.
            boot_info->memory_map.map = map;
            boot_info->memory_map.size = size_inout;
            boot_info->memory_map.descriptor_size = desc_size;
            boot_info->memory_map.descriptor_version = desc_version;
            boot_info->bootstrap.boot_services = NULL;

            serial_write_str("[mars] exit_bs: ok (attempt ");
            serial_write_dec(attempt + 1);
            serial_write_str(")\r\n");
            return EFI_SUCCESS;
        }

        if (status != EFI_INVALID_PARAMETER) {
            serial_write_str("[mars] exit_bs: fatal status ");
            serial_write_hex64((UINT64)status);
            serial_write_str("\r\n");
            (void)bs->FreePool(map);
            return status;
        }

        // EFI_INVALID_PARAMETER means the map key changed between
        // GetMemoryMap and ExitBootServices — retry the whole dance with
        // the same buffer.
        serial_write_str("[mars] exit_bs: map changed; retrying\r\n");
    }

    serial_write_str("[mars] exit_bs: gave up after retries\r\n");
    (void)bs->FreePool(map);
    return status;
}
