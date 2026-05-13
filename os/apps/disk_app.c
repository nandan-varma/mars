#include "uefi.h"
#include "app.h"
#include "input.h"
#include "process.h"
#include "scheduler.h"
#include "heap.h"
#include "vfs.h"
#include "block.h"
#include "fat.h"
#include "wm.h"
#include "internal/app_internal.h"

typedef struct {
    CHAR16 content[512];
    UINT32 content_len;
    BOOLEAN disk_present;
    UINT32 disk_size_mb;
    UINTN tick_count;
} disk_app_state_t;

static disk_app_state_t *g_disk_state = NULL;

static void update_disk_info(void) {
    if (g_disk_state == NULL) {
        return;
    }

    UINTN idx = 0;

    g_disk_state->content[idx++] = L'D';
    g_disk_state->content[idx++] = L'i';
    g_disk_state->content[idx++] = L's';
    g_disk_state->content[idx++] = L'k';
    g_disk_state->content[idx++] = L' ';
    g_disk_state->content[idx++] = L'B';
    g_disk_state->content[idx++] = L'r';
    g_disk_state->content[idx++] = L'o';
    g_disk_state->content[idx++] = L'w';
    g_disk_state->content[idx++] = L's';
    g_disk_state->content[idx++] = L'e';
    g_disk_state->content[idx++] = L'r';
    g_disk_state->content[idx++] = L':';
    g_disk_state->content[idx++] = L'\n';
    g_disk_state->content[idx++] = L'\n';

    UINTN device_count = block_get_device_count();
    if (device_count == 0) {
        g_disk_state->disk_present = FALSE;
        const CHAR16 *msg = L"No disk detected";
        for (UINTN i = 0; msg[i] != 0 && idx < 500; ++i) {
            g_disk_state->content[idx++] = msg[i];
        }
    } else {
        g_disk_state->disk_present = TRUE;
        UINT64 blocks = block_get_num_blocks(0);
        UINT32 bs = block_get_block_size(0);
        g_disk_state->disk_size_mb = (UINT32)((blocks * bs) / (1024 * 1024));

        const CHAR16 *present = L"Disk: Present\n";
        for (UINTN i = 0; present[i] != 0 && idx < 500; ++i) {
            g_disk_state->content[idx++] = present[i];
        }

        const CHAR16 *size_str = L"Size: ";
        for (UINTN i = 0; size_str[i] != 0 && idx < 500; ++i) {
            g_disk_state->content[idx++] = size_str[i];
        }

        UINT32 size = g_disk_state->disk_size_mb;
        CHAR16 size_buf[16];
        UINTN size_idx = 0;
        if (size == 0) {
            size_buf[size_idx++] = L'0';
        } else {
            CHAR16 rev[16];
            UINTN rev_idx = 0;
            while (size > 0) {
                rev[rev_idx++] = L'0' + (size % 10);
                size /= 10;
            }
            while (rev_idx > 0) {
                size_buf[size_idx++] = rev[--rev_idx];
            }
        }
        size_buf[size_idx] = 0;
        for (UINTN i = 0; size_buf[i] != 0 && idx < 500; ++i) {
            g_disk_state->content[idx++] = size_buf[i];
        }

        const CHAR16 *mb_str = L" MB\n";
        for (UINTN i = 0; mb_str[i] != 0 && idx < 500; ++i) {
            g_disk_state->content[idx++] = mb_str[i];
        }

        const CHAR16 *fs_str = L"FS: FAT16/32\n";
        for (UINTN i = 0; fs_str[i] != 0 && idx < 500; ++i) {
            g_disk_state->content[idx++] = fs_str[i];
        }

        const CHAR16 *hint = L"\nPress R to refresh";
        for (UINTN i = 0; hint[i] != 0 && idx < 500; ++i) {
            g_disk_state->content[idx++] = hint[i];
        }
    }

    g_disk_state->content_len = idx;
}

BOOLEAN disk_app_init(UINT32 window_id) {
    (void)window_id;
    g_disk_state = heap_alloc(sizeof(disk_app_state_t));
    if (g_disk_state == NULL) {
        return FALSE;
    }

    g_disk_state->content[0] = 0;
    g_disk_state->content_len = 0;
    g_disk_state->disk_present = FALSE;
    g_disk_state->disk_size_mb = 0;
    g_disk_state->tick_count = 0;

    update_disk_info();

    return TRUE;
}

void disk_app_render(app_instance_t *instance) {
    if (g_disk_state == NULL) {
        return;
    }

    g_disk_state->tick_count++;

    if (g_disk_state->tick_count % 60 == 0) {
        update_disk_info();
    }

    // Copy content to instance
    UINTN len = g_disk_state->content_len;
    if (len > APP_CONTENT_CHARS) len = APP_CONTENT_CHARS;
    for (UINTN i = 0; i < len; ++i) {
        instance->content[i] = g_disk_state->content[i];
    }
    instance->content[len] = 0;
    instance->content_len = (UINT32)len;
}

void disk_app_handle_input(const input_event_t *event) {
    if (g_disk_state == NULL || event == NULL) {
        return;
    }

    if (event->type == INPUT_EVENT_KEY_DOWN) {
        if (event->data.key.unicode == L'r' || event->data.key.unicode == L'R') {
            update_disk_info();
        }
    }
}

extern void disk_app_register(void) {
    app_manifest_t manifest = {
        L"disk_app",
        L"Disk Browser",
        CAP_INPUT | CAP_STORAGE,
        400, 100,
        380, 400
    };
    app_register(&manifest);
}