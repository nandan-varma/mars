#ifndef BOOT_INFO_H
#define BOOT_INFO_H

#include "uefi.h"

typedef struct {
    EFI_PHYSICAL_ADDRESS framebuffer_base;
    UINTN framebuffer_size;
    UINT32 width;
    UINT32 height;
    UINT32 pixels_per_scanline;

    EFI_BOOT_SERVICES *boot_services;

    EFI_MEMORY_DESCRIPTOR *memory_map;
    UINTN memory_map_size;
    UINTN memory_descriptor_size;
    UINT32 memory_descriptor_version;

    EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *text_input_ex;
    EFI_SIMPLE_POINTER_PROTOCOL *simple_pointer;
    EFI_ABSOLUTE_POINTER_PROTOCOL *absolute_pointer;
} boot_info_t;

#endif
