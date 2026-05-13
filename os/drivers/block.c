#include "block.h"
#include "platform.h"
#include "diag.h"
#include "heap.h"

#define MAX_BLOCK_DEVICES 4

typedef struct {
    EFI_HANDLE handle;
    EFI_BLOCK_IO_PROTOCOL *protocol;
    EFI_BLOCK_IO_MEDIA *media;
    BOOLEAN initialized;
} block_device_t;

static block_device_t g_devices[MAX_BLOCK_DEVICES];
static UINTN g_device_count;
static BOOLEAN g_initialized;

void block_init(void) {
    if (g_initialized) {
        return;
    }

    g_initialized = TRUE;
    g_device_count = 0;

    for (UINTN i = 0; i < MAX_BLOCK_DEVICES; ++i) {
        g_devices[i].handle = NULL;
        g_devices[i].protocol = NULL;
        g_devices[i].media = NULL;
        g_devices[i].initialized = FALSE;
    }

    diag_log(0x700U, 0, 0, 0);
}

BOOLEAN block_discover(void) {
    if (!g_initialized) {
        block_init();
    }

    EFI_BOOT_SERVICES *bs = platform_context()->boot_services;
    if (bs == NULL) {
        diag_log(0x701U, 1, 0, 0);
        return FALSE;
    }

    EFI_GUID block_io_guid = EFI_BLOCK_IO_PROTOCOL_GUID;
    EFI_STATUS status;
    UINTN num_handles = 0;
    EFI_HANDLE *handles = NULL;

    status = bs->LocateHandleBuffer(ByProtocol, &block_io_guid, NULL, &num_handles, &handles);
    if (status != EFI_SUCCESS || handles == NULL || num_handles == 0) {
        diag_log(0x702U, (UINT32)status, 0, 0);
        return FALSE;
    }

    UINTN count = num_handles < MAX_BLOCK_DEVICES ? num_handles : MAX_BLOCK_DEVICES;
    g_device_count = count;

    for (UINTN i = 0; i < count; ++i) {
        EFI_BLOCK_IO_PROTOCOL *protocol = NULL;
        status = bs->HandleProtocol(handles[i], &block_io_guid, (VOID **)&protocol);
        if (status != EFI_SUCCESS || protocol == NULL) {
            diag_log(0x703U, (UINT32)i, (UINT32)status, 0);
            continue;
        }

        g_devices[i].handle = handles[i];
        g_devices[i].protocol = protocol;
        g_devices[i].media = protocol->Media;
        g_devices[i].initialized = TRUE;

        if (g_devices[i].media != NULL) {
            diag_log(0x704U, (UINT32)i,
                g_devices[i].media->BlockSize,
                (UINT32)(g_devices[i].media->LastBlock & 0xFFFFFFFF));
        }
    }

    if (handles != NULL) {
        bs->FreePool(handles);
    }

    diag_log(0x705U, (UINT32)count, 0, 0);
    return count > 0;
}

BOOLEAN block_read(UINTN device_index, UINT64 lba, UINTN num_blocks, void *buffer) {
    if (device_index >= MAX_BLOCK_DEVICES || buffer == NULL) {
        return FALSE;
    }

    block_device_t *dev = &g_devices[device_index];
    if (!dev->initialized || dev->protocol == NULL || dev->media == NULL) {
        return FALSE;
    }

    UINTN block_size = dev->media->BlockSize;
    UINTN buffer_size = block_size * num_blocks;

    EFI_STATUS status = dev->protocol->ReadBlocks(dev->protocol, dev->media->MediaId, lba, buffer_size, buffer);
    if (status != EFI_SUCCESS) {
        diag_log(0x706U, (UINT32)device_index, (UINT32)status, 0);
        return FALSE;
    }

    return TRUE;
}

BOOLEAN block_write(UINTN device_index, UINT64 lba, UINTN num_blocks, const void *buffer) {
    if (device_index >= MAX_BLOCK_DEVICES || buffer == NULL) {
        return FALSE;
    }

    block_device_t *dev = &g_devices[device_index];
    if (!dev->initialized || dev->protocol == NULL || dev->media == NULL) {
        return FALSE;
    }

    if (dev->media->ReadOnly) {
        diag_log(0x707U, (UINT32)device_index, 0, 0);
        return FALSE;
    }

    UINTN block_size = dev->media->BlockSize;
    UINTN buffer_size = block_size * num_blocks;

    EFI_STATUS status = dev->protocol->WriteBlocks(dev->protocol, dev->media->MediaId, lba, buffer_size, (VOID *)buffer);
    if (status != EFI_SUCCESS) {
        diag_log(0x708U, (UINT32)device_index, (UINT32)status, 0);
        return FALSE;
    }

    return TRUE;
}

UINT32 block_get_block_size(UINTN device_index) {
    if (device_index >= MAX_BLOCK_DEVICES) {
        return 0;
    }

    block_device_t *dev = &g_devices[device_index];
    if (!dev->initialized || dev->media == NULL) {
        return 0;
    }

    return dev->media->BlockSize;
}

UINT64 block_get_num_blocks(UINTN device_index) {
    if (device_index >= MAX_BLOCK_DEVICES) {
        return 0;
    }

    block_device_t *dev = &g_devices[device_index];
    if (!dev->initialized || dev->media == NULL) {
        return 0;
    }

    return dev->media->LastBlock + 1;
}

BOOLEAN block_is_read_only(UINTN device_index) {
    if (device_index >= MAX_BLOCK_DEVICES) {
        return TRUE;
    }

    block_device_t *dev = &g_devices[device_index];
    if (!dev->initialized || dev->media == NULL) {
        return TRUE;
    }

    return dev->media->ReadOnly;
}

UINTN block_get_device_count(void) {
    return g_device_count;
}