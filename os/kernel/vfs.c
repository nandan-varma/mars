#include "vfs.h"

#define VFS_MAX_ENTRIES 16

typedef struct {
    const CHAR16 *path;
    const UINT8 *data;
    UINTN size;
} vfs_entry_t;

static vfs_block_device_t g_device;

static const UINT8 g_shell_manifest[] = "name=shell\nreadonly=1\n";
static const UINT8 g_files_manifest[] = "name=files\nreadonly=1\n";
static const UINT8 g_term_manifest[] = "name=term\nreadonly=1\n";

static const vfs_entry_t g_entries[VFS_MAX_ENTRIES] = {
    { L"/apps/shell.manifest", g_shell_manifest, sizeof(g_shell_manifest) - 1 },
    { L"/apps/files.manifest", g_files_manifest, sizeof(g_files_manifest) - 1 },
    { L"/apps/term.manifest", g_term_manifest, sizeof(g_term_manifest) - 1 }
};

static BOOLEAN path_equals(const CHAR16 *a, const CHAR16 *b) {
    if (a == NULL || b == NULL) {
        return FALSE;
    }

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

void vfs_init(void) {
    g_device.block_size = 512;
    g_device.block_count = 0;
    g_device.read_only = TRUE;
}

void vfs_mount_boot_device(vfs_block_device_t device) {
    g_device = device;
}

BOOLEAN vfs_exists(const CHAR16 *path) {
    for (UINTN i = 0; i < VFS_MAX_ENTRIES; ++i) {
        if (g_entries[i].path == NULL) {
            continue;
        }
        if (path_equals(g_entries[i].path, path)) {
            return TRUE;
        }
    }
    return FALSE;
}

UINTN vfs_read(const CHAR16 *path, UINT8 *out_buffer, UINTN max_bytes) {
    if (path == NULL || out_buffer == NULL || max_bytes == 0) {
        return 0;
    }

    for (UINTN i = 0; i < VFS_MAX_ENTRIES; ++i) {
        if (g_entries[i].path == NULL || !path_equals(g_entries[i].path, path)) {
            continue;
        }

        UINTN count = g_entries[i].size < max_bytes ? g_entries[i].size : max_bytes;
        for (UINTN j = 0; j < count; ++j) {
            out_buffer[j] = g_entries[i].data[j];
        }
        return count;
    }

    return 0;
}