#include "platform.h"

static platform_context_t g_platform;

void platform_init_from_boot(const boot_info_t *boot_info) {
    if (boot_info == NULL) {
        return;
    }

    g_platform.framebuffer = boot_info->framebuffer;
    g_platform.memory_map = boot_info->memory_map;
    g_platform.input = boot_info->input;
}

const platform_context_t *platform_context(void) {
    return &g_platform;
}