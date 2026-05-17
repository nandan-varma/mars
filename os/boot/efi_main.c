#include "uefi.h"

#include "boot_info.h"
#include "exit_boot_services.h"
#include "kernel.h"
#include "serial.h"

static EFI_STATUS find_gop(EFI_BOOT_SERVICES *bs, EFI_GRAPHICS_OUTPUT_PROTOCOL **gop_out) {
    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    return bs->LocateProtocol(&gop_guid, NULL, (void **)gop_out);
}

static EFI_STATUS find_text_input_ex(EFI_BOOT_SERVICES *bs, EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL **out) {
    EFI_GUID guid = EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL_GUID;
    return bs->LocateProtocol(&guid, NULL, (void **)out);
}

static BOOLEAN is_simple_pointer_usable(EFI_SIMPLE_POINTER_PROTOCOL *pointer) {
    return pointer != NULL && pointer->Mode != NULL;
}

static BOOLEAN is_absolute_pointer_usable(EFI_ABSOLUTE_POINTER_PROTOCOL *pointer) {
    if (pointer == NULL || pointer->Mode == NULL) {
        return FALSE;
    }

    return pointer->Mode->AbsoluteMaxX > pointer->Mode->AbsoluteMinX
        && pointer->Mode->AbsoluteMaxY > pointer->Mode->AbsoluteMinY;
}

static EFI_STATUS find_simple_pointer(EFI_SYSTEM_TABLE *st, EFI_SIMPLE_POINTER_PROTOCOL **out) {
    EFI_BOOT_SERVICES *bs = st->BootServices;
    EFI_GUID guid = EFI_SIMPLE_POINTER_PROTOCOL_GUID;

    EFI_SIMPLE_POINTER_PROTOCOL *console_candidate = NULL;
    if (!EFI_ERROR(bs->HandleProtocol(st->ConsoleInHandle, &guid, (void **)&console_candidate))
        && is_simple_pointer_usable(console_candidate)) {
        *out = console_candidate;
        return EFI_SUCCESS;
    }

    EFI_HANDLE *handles = NULL;
    UINTN handle_count = 0;
    EFI_STATUS status = bs->LocateHandleBuffer(ByProtocol, &guid, NULL, &handle_count, &handles);
    if (!EFI_ERROR(status) && handles != NULL) {
        for (UINTN i = 0; i < handle_count; ++i) {
            EFI_SIMPLE_POINTER_PROTOCOL *candidate = NULL;
            if (!EFI_ERROR(bs->HandleProtocol(handles[i], &guid, (void **)&candidate))
                && is_simple_pointer_usable(candidate)) {
                *out = candidate;
                (void)bs->FreePool(handles);
                return EFI_SUCCESS;
            }
        }
        (void)bs->FreePool(handles);
    }

    EFI_SIMPLE_POINTER_PROTOCOL *global_candidate = NULL;
    status = bs->LocateProtocol(&guid, NULL, (void **)&global_candidate);
    if (!EFI_ERROR(status) && is_simple_pointer_usable(global_candidate)) {
        *out = global_candidate;
        return EFI_SUCCESS;
    }

    return status;
}

static EFI_STATUS find_absolute_pointer(EFI_SYSTEM_TABLE *st, EFI_ABSOLUTE_POINTER_PROTOCOL **out) {
    EFI_BOOT_SERVICES *bs = st->BootServices;
    EFI_GUID guid = EFI_ABSOLUTE_POINTER_PROTOCOL_GUID;

    EFI_ABSOLUTE_POINTER_PROTOCOL *console_candidate = NULL;
    if (!EFI_ERROR(bs->HandleProtocol(st->ConsoleInHandle, &guid, (void **)&console_candidate))
        && is_absolute_pointer_usable(console_candidate)) {
        *out = console_candidate;
        return EFI_SUCCESS;
    }

    EFI_HANDLE *handles = NULL;
    UINTN handle_count = 0;
    EFI_STATUS status = bs->LocateHandleBuffer(ByProtocol, &guid, NULL, &handle_count, &handles);
    if (!EFI_ERROR(status) && handles != NULL) {
        for (UINTN i = 0; i < handle_count; ++i) {
            EFI_ABSOLUTE_POINTER_PROTOCOL *candidate = NULL;
            if (!EFI_ERROR(bs->HandleProtocol(handles[i], &guid, (void **)&candidate))
                && is_absolute_pointer_usable(candidate)) {
                *out = candidate;
                (void)bs->FreePool(handles);
                return EFI_SUCCESS;
            }
        }
        (void)bs->FreePool(handles);
    }

    EFI_ABSOLUTE_POINTER_PROTOCOL *global_candidate = NULL;
    status = bs->LocateProtocol(&guid, NULL, (void **)&global_candidate);
    if (!EFI_ERROR(status) && is_absolute_pointer_usable(global_candidate)) {
        *out = global_candidate;
        return EFI_SUCCESS;
    }

    return status;
}

static EFI_STATUS collect_memory_map(
    EFI_BOOT_SERVICES *bs,
    EFI_MEMORY_DESCRIPTOR **memory_map_out,
    UINTN *memory_map_size_out,
    UINTN *descriptor_size_out,
    UINT32 *descriptor_version_out
) {
    UINTN memory_map_size = 0;
    EFI_MEMORY_DESCRIPTOR *memory_map = NULL;
    UINTN map_key = 0;
    UINTN descriptor_size = 0;
    UINT32 descriptor_version = 0;

    EFI_STATUS status = bs->GetMemoryMap(
        &memory_map_size,
        memory_map,
        &map_key,
        &descriptor_size,
        &descriptor_version
    );

    if (status != EFI_BUFFER_TOO_SMALL) {
        return status;
    }

    memory_map_size += descriptor_size * 4;
    status = bs->AllocatePool(EfiLoaderData, memory_map_size, (void **)&memory_map);
    if (EFI_ERROR(status)) {
        return status;
    }

    status = bs->GetMemoryMap(
        &memory_map_size,
        memory_map,
        &map_key,
        &descriptor_size,
        &descriptor_version
    );
    if (EFI_ERROR(status)) {
        bs->FreePool(memory_map);
        return status;
    }

    *memory_map_out = memory_map;
    *memory_map_size_out = memory_map_size;
    *descriptor_size_out = descriptor_size;
    *descriptor_version_out = descriptor_version;
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *system_table) {
    (void)image_handle;
    serial_init();
    serial_write_str("[mars] efi_main:start\r\n");
    EFI_BOOT_SERVICES *bs = system_table->BootServices;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
    EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *text_input_ex = NULL;
    EFI_SIMPLE_POINTER_PROTOCOL *simple_pointer = NULL;
    EFI_ABSOLUTE_POINTER_PROTOCOL *absolute_pointer = NULL;
    EFI_MEMORY_DESCRIPTOR *memory_map = NULL;
    UINTN memory_map_size = 0;
    UINTN descriptor_size = 0;
    UINT32 descriptor_version = 0;

    EFI_STATUS status = find_gop(bs, &gop);
    if (EFI_ERROR(status)) {
        serial_write_str("[mars] efi_main:fail:gop\r\n");
        return status;
    }

    (void)find_text_input_ex(bs, &text_input_ex);

    (void)find_absolute_pointer(system_table, &absolute_pointer);
    (void)find_simple_pointer(system_table, &simple_pointer);

    if (simple_pointer != NULL) {
        (void)simple_pointer->Reset(simple_pointer, TRUE);
    }
    if (absolute_pointer != NULL) {
        (void)absolute_pointer->Reset(absolute_pointer, TRUE);
    }

    status = collect_memory_map(
        bs,
        &memory_map,
        &memory_map_size,
        &descriptor_size,
        &descriptor_version
    );
    if (EFI_ERROR(status)) {
        serial_write_str("[mars] efi_main:fail:memmap\r\n");
        return status;
    }

    boot_info_t boot_info;
    boot_info.framebuffer.base = gop->Mode->FrameBufferBase;
    boot_info.framebuffer.size = gop->Mode->FrameBufferSize;
    boot_info.framebuffer.width = gop->Mode->Info->HorizontalResolution;
    boot_info.framebuffer.height = gop->Mode->Info->VerticalResolution;
    boot_info.framebuffer.pixels_per_scanline = gop->Mode->Info->PixelsPerScanLine;
    boot_info.bootstrap.boot_services = bs;
    boot_info.memory_map.map = memory_map;
    boot_info.memory_map.size = memory_map_size;
    boot_info.memory_map.descriptor_size = descriptor_size;
    boot_info.memory_map.descriptor_version = descriptor_version;
    boot_info.input.text_input_ex = text_input_ex;
    boot_info.input.simple_pointer = simple_pointer;
    boot_info.input.absolute_pointer = absolute_pointer;
    boot_info.runtime.runtime_services = system_table->RuntimeServices;

#ifdef MARS_EXIT_BOOT_SERVICES
    serial_write_str("[mars] efi_main:exit_bs\r\n");
    EFI_STATUS exit_status = exit_boot_services(image_handle, system_table, &boot_info);
    if (EFI_ERROR(exit_status)) {
        serial_write_str("[mars] efi_main:exit_bs:fail\r\n");
        return exit_status;
    }
#endif

    serial_write_str("[mars] efi_main:handoff\r\n");
    kernel_main(&boot_info);
    serial_write_str("[mars] efi_main:return\r\n");
    return EFI_SUCCESS;
}
