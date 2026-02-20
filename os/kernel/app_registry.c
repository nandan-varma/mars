#include "app_internal.h"

static app_manifest_t g_manifests[MAX_APPS];
static UINTN g_manifest_count;

void app_registry_init(void) {
    g_manifest_count = 0;
}

BOOLEAN app_registry_register(const app_manifest_t *manifest) {
    if (manifest == NULL || g_manifest_count >= MAX_APPS) {
        return FALSE;
    }

    g_manifests[g_manifest_count] = *manifest;
    ++g_manifest_count;
    return TRUE;
}

UINTN app_registry_count(void) {
    return g_manifest_count;
}

BOOLEAN app_registry_manifest_at(UINTN index, app_manifest_t *out_manifest) {
    if (out_manifest == NULL || index >= g_manifest_count) {
        return FALSE;
    }

    *out_manifest = g_manifests[index];
    return TRUE;
}
