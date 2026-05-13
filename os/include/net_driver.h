#ifndef NET_DRIVER_H
#define NET_DRIVER_H

#include "uefi.h"

#define NET_DRIVER_MAX_DEVICES 2
#define NET_DRIVER_MAC_ADDR_LEN 6
#define NET_DRIVER_PACKET_MAX_SIZE 1514

typedef struct {
    EFI_HANDLE handle;
    EFI_SIMPLE_NETWORK_PROTOCOL *snp;
    UINT8 mac_addr[NET_DRIVER_MAC_ADDR_LEN];
    BOOLEAN initialized;
    BOOLEAN started;
} net_driver_device_t;

void net_driver_init(void);
BOOLEAN net_driver_discover(void);
UINTN net_driver_get_device_count(void);
BOOLEAN net_driver_get_mac_address(UINTN device_index, UINT8 *mac_out);
UINTN net_driver_receive(UINTN device_index, UINT8 *buffer, UINTN max_size);
BOOLEAN net_driver_transmit(UINTN device_index, const UINT8 *data, UINTN size);

#endif