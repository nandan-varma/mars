#ifndef BLOCK_H
#define BLOCK_H

#include "uefi.h"

void block_init(void);
BOOLEAN block_discover(void);
BOOLEAN block_read(UINTN device_index, UINT64 lba, UINTN num_blocks, void *buffer);
BOOLEAN block_write(UINTN device_index, UINT64 lba, UINTN num_blocks, const void *buffer);
UINT32 block_get_block_size(UINTN device_index);
UINT64 block_get_num_blocks(UINTN device_index);
BOOLEAN block_is_read_only(UINTN device_index);
UINTN block_get_device_count(void);

#endif