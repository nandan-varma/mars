#include "vfs.h"
#include "block.h"
#include "fat.h"
#include "diag.h"

#define VFS_MAX_ENTRIES 16
#define VFS_MAX_PATH_LEN 64

typedef struct {
    const CHAR16 *path;
    const UINT8 *data;
    UINTN size;
    BOOLEAN is_embedded;
} vfs_entry_t;

static vfs_block_device_t g_device;
static BOOLEAN g_block_mounted;
static vfs_entry_t g_embedded_entries[VFS_MAX_ENTRIES];

void vfs_init(void) {
    g_device.block_size = 512;
    g_device.block_count = 0;
    g_device.read_only = TRUE;
    g_block_mounted = FALSE;

    for (UINTN i = 0; i < VFS_MAX_ENTRIES; ++i) {
        g_embedded_entries[i].path = NULL;
        g_embedded_entries[i].data = NULL;
        g_embedded_entries[i].size = 0;
        g_embedded_entries[i].is_embedded = FALSE;
    }

    diag_log(0x100U, 0, 0, 0);
}

void vfs_mount_boot_device(vfs_block_device_t device) {
    g_device = device;
}

BOOLEAN vfs_mount_block_device(UINTN device_index) {
    if (g_block_mounted) {
        return TRUE;
    }

    if (!block_discover()) {
        diag_log(0x101U, 1, device_index, 0);
        return FALSE;
    }

    if (device_index >= block_get_device_count()) {
        diag_log(0x101U, 2, device_index, block_get_device_count());
        return FALSE;
    }

    if (!fat_init()) {
        diag_log(0x101U, 3, 0, 0);
        return FALSE;
    }

    if (!fat_mount(device_index)) {
        diag_log(0x101U, 4, 0, 0);
        return FALSE;
    }

    g_device.block_size = block_get_block_size(device_index);
    g_device.block_count = block_get_num_blocks(device_index);
    g_device.read_only = block_is_read_only(device_index);
    g_block_mounted = TRUE;

    diag_log(0x102U, g_device.block_size, (UINT32)g_device.block_count, g_device.read_only);
    return TRUE;
}

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
        if (i >= VFS_MAX_PATH_LEN) {
            return FALSE;
        }
    }
}

static void convert_to_fat_name(const CHAR16 *unicode_path, CHAR8 *fat_name) {
    UINTN i = 0;
    UINTN fat_idx = 0;

    for (; fat_idx < 8 && unicode_path[i] != 0 && unicode_path[i] != L'.'; ++fat_idx, ++i) {
        if (unicode_path[i] >= L'a' && unicode_path[i] <= L'z') {
            fat_name[fat_idx] = (CHAR8)(unicode_path[i] - L'a' + L'A');
        } else {
            fat_name[fat_idx] = (CHAR8)unicode_path[i];
        }
    }

    while (fat_idx < 8) {
        fat_name[fat_idx++] = ' ';
    }

    if (unicode_path[i] == L'.') {
        ++i;
    }

    for (fat_idx = 8; fat_idx < 11 && unicode_path[i] != 0; ++fat_idx, ++i) {
        if (unicode_path[i] >= L'a' && unicode_path[i] <= L'z') {
            fat_name[fat_idx] = (CHAR8)(unicode_path[i] - L'a' + L'A');
        } else {
            fat_name[fat_idx] = (CHAR8)unicode_path[i];
        }
    }

    while (fat_idx < 11) {
        fat_name[fat_idx++] = ' ';
    }
}

BOOLEAN vfs_exists(const CHAR16 *path) {
    if (path == NULL) {
        return FALSE;
    }

    for (UINTN i = 0; i < VFS_MAX_ENTRIES; ++i) {
        if (g_embedded_entries[i].path == NULL) {
            continue;
        }
        if (g_embedded_entries[i].is_embedded && path_equals(g_embedded_entries[i].path, path)) {
            return TRUE;
        }
    }

    if (g_block_mounted) {
        CHAR8 any_file[] = "*";
        return fat_read_file(any_file, NULL, 0, NULL);
    }

    return FALSE;
}

UINTN vfs_read(const CHAR16 *path, UINT8 *out_buffer, UINTN max_bytes) {
    if (path == NULL || out_buffer == NULL || max_bytes == 0) {
        return 0;
    }

    for (UINTN i = 0; i < VFS_MAX_ENTRIES; ++i) {
        if (g_embedded_entries[i].path == NULL) {
            continue;
        }
        if (g_embedded_entries[i].is_embedded && path_equals(g_embedded_entries[i].path, path)) {
            UINTN count = g_embedded_entries[i].size < max_bytes ? g_embedded_entries[i].size : max_bytes;
            for (UINTN j = 0; j < count; ++j) {
                out_buffer[j] = g_embedded_entries[i].data[j];
            }
            return count;
        }
    }

    if (g_block_mounted) {
        CHAR8 fat_name[12];
        convert_to_fat_name(path, fat_name);

        UINTN out_size = 0;
        if (fat_read_file(fat_name, out_buffer, max_bytes, &out_size)) {
            return out_size;
        }
    }

    return 0;
}