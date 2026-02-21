#include "platform.h"

#include "memory.h"

static platform_context_t g_platform;
static BOOLEAN g_platform_initialized = FALSE;

BOOLEAN platform_init_from_boot(const boot_info_t *boot_info) {
    if (boot_info == NULL) {
        return FALSE;
    }

    g_platform.framebuffer = boot_info->framebuffer;
    g_platform.memory_map = boot_info->memory_map;
    g_platform.input = boot_info->input;
    g_platform.boot_services = boot_info->bootstrap.boot_services;
    g_platform.runtime_services = boot_info->runtime.runtime_services;
    g_platform_initialized = TRUE;
    return TRUE;
}

const platform_context_t *platform_context(void) {
    return &g_platform;
}

BOOLEAN platform_is_initialized(void) {
    return g_platform_initialized;
}