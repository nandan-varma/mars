#include "platform.h"

static platform_context_t g_platform;

void platform_init_from_boot(const boot_info_t *boot_info) {
    if (boot_info == NULL) {
        return;
    }

    g_platform.framebuffer = boot_info->framebuffer;
    g_platform.memory_map = boot_info->memory_map;
    g_platform.input = boot_info->input;
    g_platform.boot_services = boot_info->bootstrap.boot_services;
    g_platform.runtime_services = boot_info->runtime.runtime_services;
}

const platform_context_t *platform_context(void) {
    return &g_platform;
}