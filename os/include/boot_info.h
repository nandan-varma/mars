#ifndef BOOT_INFO_H
#define BOOT_INFO_H

#include "uefi.h"

typedef struct {
    EFI_PHYSICAL_ADDRESS base;
    UINTN size;
    UINT32 width;
    UINT32 height;
    UINT32 pixels_per_scanline;
} boot_framebuffer_t;

typedef struct {
    EFI_MEMORY_DESCRIPTOR *map;
    UINTN size;
    UINTN descriptor_size;
    UINT32 descriptor_version;
} boot_memory_map_t;

typedef struct {
    EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *text_input_ex;
    EFI_SIMPLE_POINTER_PROTOCOL *simple_pointer;
    EFI_ABSOLUTE_POINTER_PROTOCOL *absolute_pointer;
} boot_input_handles_t;

typedef struct {
    EFI_BOOT_SERVICES *boot_services;
} boot_bootstrap_services_t;

typedef struct {
    boot_framebuffer_t framebuffer;
    boot_memory_map_t memory_map;
    boot_input_handles_t input;
    boot_bootstrap_services_t bootstrap;
} boot_info_t;

#endif
