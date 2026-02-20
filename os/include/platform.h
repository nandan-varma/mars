#ifndef PLATFORM_H
#define PLATFORM_H

#include "boot_info.h"

typedef struct {
    boot_framebuffer_t framebuffer;
    boot_memory_map_t memory_map;
    boot_input_handles_t input;
} platform_context_t;

void platform_init_from_boot(const boot_info_t *boot_info);
const platform_context_t *platform_context(void);

#endif