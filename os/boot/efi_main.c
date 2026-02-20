#include "uefi.h"

#include "boot_info.h"
#include "kernel.h"

#ifndef EXIT_BOOT_SERVICES
#define EXIT_BOOT_SERVICES 0
#endif

static EFI_STATUS find_gop(EFI_BOOT_SERVICES *bs, EFI_GRAPHICS_OUTPUT_PROTOCOL **gop_out) {
    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    return bs->LocateProtocol(&gop_guid, NULL, (void **)gop_out);
}

static EFI_STATUS find_text_input_ex(EFI_BOOT_SERVICES *bs, EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL **out) {
    EFI_GUID guid = EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL_GUID;
    return bs->LocateProtocol(&guid, NULL, (void **)out);
}

static EFI_STATUS find_simple_pointer(EFI_BOOT_SERVICES *bs, EFI_SIMPLE_POINTER_PROTOCOL **out) {
    EFI_GUID guid = EFI_SIMPLE_POINTER_PROTOCOL_GUID;

    EFI_HANDLE *handles = NULL;
    UINTN handle_count = 0;
    EFI_STATUS status = bs->LocateHandleBuffer(ByProtocol, &guid, NULL, &handle_count, &handles);
    if (!EFI_ERROR(status) && handles != NULL) {
        for (UINTN i = 0; i < handle_count; ++i) {
            EFI_SIMPLE_POINTER_PROTOCOL *candidate = NULL;
            if (!EFI_ERROR(bs->HandleProtocol(handles[i], &guid, (void **)&candidate)) && candidate != NULL) {
                *out = candidate;
                (void)bs->FreePool(handles);
                return EFI_SUCCESS;
            }
        }
        (void)bs->FreePool(handles);
    }

    return bs->LocateProtocol(&guid, NULL, (void **)out);
}

static EFI_STATUS find_absolute_pointer(EFI_BOOT_SERVICES *bs, EFI_ABSOLUTE_POINTER_PROTOCOL **out) {
    EFI_GUID guid = EFI_ABSOLUTE_POINTER_PROTOCOL_GUID;

    EFI_HANDLE *handles = NULL;
    UINTN handle_count = 0;
    EFI_STATUS status = bs->LocateHandleBuffer(ByProtocol, &guid, NULL, &handle_count, &handles);
    if (!EFI_ERROR(status) && handles != NULL) {
        for (UINTN i = 0; i < handle_count; ++i) {
            EFI_ABSOLUTE_POINTER_PROTOCOL *candidate = NULL;
            if (!EFI_ERROR(bs->HandleProtocol(handles[i], &guid, (void **)&candidate)) && candidate != NULL) {
                *out = candidate;
                (void)bs->FreePool(handles);
                return EFI_SUCCESS;
            }
        }
        (void)bs->FreePool(handles);
    }

    return bs->LocateProtocol(&guid, NULL, (void **)out);
}

static EFI_STATUS find_simple_pointer_from_console(EFI_SYSTEM_TABLE *st, EFI_SIMPLE_POINTER_PROTOCOL **out) {
    EFI_GUID guid = EFI_SIMPLE_POINTER_PROTOCOL_GUID;
    return st->BootServices->HandleProtocol(st->ConsoleInHandle, &guid, (void **)out);
}

static EFI_STATUS find_absolute_pointer_from_console(EFI_SYSTEM_TABLE *st, EFI_ABSOLUTE_POINTER_PROTOCOL **out) {
    EFI_GUID guid = EFI_ABSOLUTE_POINTER_PROTOCOL_GUID;
    return st->BootServices->HandleProtocol(st->ConsoleInHandle, &guid, (void **)out);
}

static EFI_STATUS maybe_exit_boot_services(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *system_table) {
#if EXIT_BOOT_SERVICES
    EFI_BOOT_SERVICES *bs = system_table->BootServices;
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

    status = bs->ExitBootServices(image_handle, map_key);
    if (EFI_ERROR(status)) {
        status = bs->GetMemoryMap(
            &memory_map_size,
            memory_map,
            &map_key,
            &descriptor_size,
            &descriptor_version
        );
        if (!EFI_ERROR(status)) {
            status = bs->ExitBootServices(image_handle, map_key);
        }
    }

    return status;
#else
    (void)image_handle;
    (void)system_table;
    return EFI_SUCCESS;
#endif
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *system_table) {
    EFI_BOOT_SERVICES *bs = system_table->BootServices;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
    EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *text_input_ex = NULL;
    EFI_SIMPLE_POINTER_PROTOCOL *simple_pointer = NULL;
    EFI_ABSOLUTE_POINTER_PROTOCOL *absolute_pointer = NULL;

    EFI_STATUS status = find_gop(bs, &gop);
    if (EFI_ERROR(status)) {
        return status;
    }

    (void)find_text_input_ex(bs, &text_input_ex);

    if (EFI_ERROR(find_simple_pointer_from_console(system_table, &simple_pointer))) {
        (void)find_simple_pointer(bs, &simple_pointer);
    }

    if (EFI_ERROR(find_absolute_pointer_from_console(system_table, &absolute_pointer))) {
        (void)find_absolute_pointer(bs, &absolute_pointer);
    }

    if (simple_pointer != NULL) {
        (void)simple_pointer->Reset(simple_pointer, TRUE);
    }
    if (absolute_pointer != NULL) {
        (void)absolute_pointer->Reset(absolute_pointer, TRUE);
    }

    boot_info_t boot_info;
    boot_info.framebuffer_base = gop->Mode->FrameBufferBase;
    boot_info.framebuffer_size = gop->Mode->FrameBufferSize;
    boot_info.width = gop->Mode->Info->HorizontalResolution;
    boot_info.height = gop->Mode->Info->VerticalResolution;
    boot_info.pixels_per_scanline = gop->Mode->Info->PixelsPerScanLine;
    boot_info.system_table = system_table;
    boot_info.boot_services = bs;
    boot_info.runtime_services = system_table->RuntimeServices;
    boot_info.text_input_ex = text_input_ex;
    boot_info.simple_pointer = simple_pointer;
    boot_info.absolute_pointer = absolute_pointer;

    status = maybe_exit_boot_services(image_handle, system_table);
    if (EFI_ERROR(status)) {
        return status;
    }

#if EXIT_BOOT_SERVICES
    boot_info.boot_services = NULL;
    boot_info.text_input_ex = NULL;
    boot_info.simple_pointer = NULL;
    boot_info.absolute_pointer = NULL;
#endif

    kernel_main(&boot_info);
    return EFI_SUCCESS;
}
