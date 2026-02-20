#include "app_internal.h"

#include "heap.h"
#include "process.h"
#include "wm.h"

static app_instance_t *g_instances[MAX_APPS];
static UINTN g_instance_count;

static BOOLEAN app_equals(const CHAR16 *a, const CHAR16 *b) {
    UINTN i = 0;
    for (;;) {
        if (a[i] != b[i]) {
            return FALSE;
        }
        if (a[i] == 0) {
            return TRUE;
        }
        ++i;
    }
}

void app_instance_init(void) {
    g_instance_count = 0;
    for (UINTN i = 0; i < MAX_APPS; ++i) {
        g_instances[i] = NULL;
    }
}

BOOLEAN app_instance_launch(const CHAR16 *id, app_task_entry_t entry, app_content_init_t content_init) {
    if (id == NULL || entry == NULL || content_init == NULL) {
        return FALSE;
    }

    if (g_instance_count >= MAX_APPS) {
        return FALSE;
    }

    for (UINTN i = 0; i < g_instance_count; ++i) {
        app_instance_t *instance = g_instances[i];
        if (instance == NULL) {
            continue;
        }

        if (!app_equals(instance->manifest.id, id)) {
            continue;
        }

        if (process_is_running(instance->pid)) {
            return wm_focus_window(instance->window_id);
        }
    }

    for (UINTN i = 0; i < app_registry_count(); ++i) {
        app_manifest_t manifest;
        if (!app_registry_manifest_at(i, &manifest)) {
            continue;
        }

        if (!app_equals(manifest.id, id)) {
            continue;
        }

        app_instance_t *instance = (app_instance_t *)heap_alloc(sizeof(app_instance_t));
        if (instance == NULL) {
            return FALSE;
        }

        instance->manifest = manifest;
        instance->ticks = 0;
        instance->content[0] = 0;
        instance->content_len = 0;
        instance->history[0] = 0;
        instance->history_len = 0;
        instance->input_line[0] = 0;
        instance->input_len = 0;
        instance->pid = process_create_kernel(instance->manifest.title, entry, instance, 1, instance->manifest.capabilities);
        if (instance->pid == 0) {
            return FALSE;
        }

        instance->window_id = wm_create_window(
            instance->pid,
            instance->manifest.title,
            instance->manifest.start_x,
            instance->manifest.start_y,
            instance->manifest.width,
            instance->manifest.height
        );
        if (instance->window_id == 0) {
            process_exit(instance->pid, -2);
            return FALSE;
        }

        content_init(instance);
        (void)wm_set_window_content(instance->window_id, instance->content);

        g_instances[g_instance_count++] = instance;

        return instance->window_id != 0;
    }

    return FALSE;
}
