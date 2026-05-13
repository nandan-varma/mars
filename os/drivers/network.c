#include "net_driver.h"
#include "platform.h"
#include "diag.h"
#include "heap.h"

static net_driver_device_t g_devices[NET_DRIVER_MAX_DEVICES];
static UINTN g_device_count;
static BOOLEAN g_initialized;
static UINT8 *g_rx_buffer;

void net_driver_init(void) {
    if (g_initialized) {
        return;
    }

    g_initialized = TRUE;
    g_device_count = 0;
    g_rx_buffer = heap_alloc(NET_DRIVER_PACKET_MAX_SIZE);

    for (UINTN i = 0; i < NET_DRIVER_MAX_DEVICES; ++i) {
        g_devices[i].handle = NULL;
        g_devices[i].snp = NULL;
        g_devices[i].initialized = FALSE;
        g_devices[i].started = FALSE;
        for (UINTN j = 0; j < NET_DRIVER_MAC_ADDR_LEN; ++j) {
            g_devices[i].mac_addr[j] = 0;
        }
    }

    diag_log(0x800U, 0, 0, 0);
}

BOOLEAN net_driver_discover(void) {
    if (!g_initialized) {
        net_driver_init();
    }

    EFI_BOOT_SERVICES *bs = platform_context()->boot_services;
    if (bs == NULL) {
        diag_log(0x801U, 1, 0, 0);
        return FALSE;
    }

    EFI_GUID snp_guid = EFI_SIMPLE_NETWORK_PROTOCOL_GUID;
    EFI_STATUS status;
    UINTN num_handles = 0;
    EFI_HANDLE *handles = NULL;

    status = bs->LocateHandleBuffer(ByProtocol, &snp_guid, NULL, &num_handles, &handles);
    if (status != EFI_SUCCESS || handles == NULL || num_handles == 0) {
        diag_log(0x802U, (UINT32)status, 0, 0);
        return FALSE;
    }

    UINTN count = num_handles < NET_DRIVER_MAX_DEVICES ? num_handles : NET_DRIVER_MAX_DEVICES;
    g_device_count = count;

    for (UINTN i = 0; i < count; ++i) {
        EFI_SIMPLE_NETWORK_PROTOCOL *snp = NULL;
        status = bs->HandleProtocol(handles[i], &snp_guid, (VOID **)&snp);
        if (status != EFI_SUCCESS || snp == NULL) {
            diag_log(0x803U, (UINT32)i, (UINT32)status, 0);
            continue;
        }

        status = snp->Open(snp, NULL);
        if (status != EFI_SUCCESS) {
            diag_log(0x804U, (UINT32)i, (UINT32)status, 0);
            continue;
        }

        status = snp->Initialize(snp, NET_DRIVER_PACKET_MAX_SIZE, NET_DRIVER_PACKET_MAX_SIZE, NET_DRIVER_PACKET_MAX_SIZE);
        if (status != EFI_SUCCESS) {
            snp->Close(snp);
            diag_log(0x805U, (UINT32)i, (UINT32)status, 0);
            continue;
        }

        g_devices[i].handle = handles[i];
        g_devices[i].snp = snp;
        g_devices[i].initialized = TRUE;
        g_devices[i].started = TRUE;

        UINT8 *mac = (UINT8 *)((UINT8 *)snp->Mode + 20);
        for (UINTN j = 0; j < NET_DRIVER_MAC_ADDR_LEN; ++j) {
            g_devices[i].mac_addr[j] = mac[j];
        }

        diag_log(0x806U, (UINT32)i,
            (g_devices[i].mac_addr[0] << 8) | g_devices[i].mac_addr[1],
            (g_devices[i].mac_addr[2] << 8) | g_devices[i].mac_addr[3]);
    }

    if (handles != NULL) {
        bs->FreePool(handles);
    }

    diag_log(0x807U, (UINT32)count, 0, 0);
    return count > 0;
}

UINTN net_driver_get_device_count(void) {
    return g_device_count;
}

BOOLEAN net_driver_get_mac_address(UINTN device_index, UINT8 *mac_out) {
    if (device_index >= NET_DRIVER_MAX_DEVICES || mac_out == NULL) {
        return FALSE;
    }

    net_driver_device_t *dev = &g_devices[device_index];
    if (!dev->initialized || !dev->started) {
        return FALSE;
    }

    for (UINTN i = 0; i < NET_DRIVER_MAC_ADDR_LEN; ++i) {
        mac_out[i] = dev->mac_addr[i];
    }
    return TRUE;
}

UINTN net_driver_receive(UINTN device_index, UINT8 *buffer, UINTN max_size) {
    if (device_index >= NET_DRIVER_MAX_DEVICES || buffer == NULL || max_size == 0) {
        return 0;
    }

    net_driver_device_t *dev = &g_devices[device_index];
    if (!dev->initialized || !dev->started || dev->snp == NULL) {
        return 0;
    }

    UINTN header_size = 0;
    UINTN buffer_size = max_size < NET_DRIVER_PACKET_MAX_SIZE ? max_size : NET_DRIVER_PACKET_MAX_SIZE;

    EFI_STATUS status = dev->snp->Receive(dev->snp, &header_size, &buffer_size, buffer, NULL);

    if (status != EFI_SUCCESS) {
        return 0;
    }

    return buffer_size;
}

BOOLEAN net_driver_transmit(UINTN device_index, const UINT8 *data, UINTN size) {
    if (device_index >= NET_DRIVER_MAX_DEVICES || data == NULL || size == 0) {
        return FALSE;
    }

    net_driver_device_t *dev = &g_devices[device_index];
    if (!dev->initialized || !dev->started || dev->snp == NULL) {
        return FALSE;
    }

    if (size > NET_DRIVER_PACKET_MAX_SIZE) {
        size = NET_DRIVER_PACKET_MAX_SIZE;
    }

    EFI_STATUS status = dev->snp->Transmit(dev->snp, 0, size, (VOID *)data, NULL);

    if (status != EFI_SUCCESS) {
        diag_log(0x808U, (UINT32)device_index, (UINT32)status, size);
        return FALSE;
    }

    return TRUE;
}