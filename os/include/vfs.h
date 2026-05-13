#ifndef VFS_H
#define VFS_H

#include "uefi.h"

typedef struct {
    UINT32 block_size;
    UINT32 block_count;
    BOOLEAN read_only;
} vfs_block_device_t;

void vfs_init(void);
void vfs_mount_boot_device(vfs_block_device_t device);
BOOLEAN vfs_mount_block_device(UINTN device_index);
BOOLEAN vfs_exists(const CHAR16 *path);
UINTN vfs_read(const CHAR16 *path, UINT8 *out_buffer, UINTN max_bytes);

#endif