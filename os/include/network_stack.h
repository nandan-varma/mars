#ifndef NETWORK_STACK_H
#define NETWORK_STACK_H

#include "uefi.h"

#define NET_IP_ADDR_LEN 4
#define NET_ETH_ADDR_LEN 6
#define NET_TCP_MAX_PORTS 8
#define NET_PACKET_SIZE 1514

typedef struct {
    UINT8 addr[NET_IP_ADDR_LEN];
} ip_addr_t;

typedef struct {
    UINT8 addr[NET_ETH_ADDR_LEN];
} eth_addr_t;

typedef struct {
    UINT8 dest[NET_ETH_ADDR_LEN];
    UINT8 src[NET_ETH_ADDR_LEN];
    UINT16 type;
} __attribute__((packed)) eth_header_t;

typedef struct {
    UINT8 version_ihl;
    UINT8 tos;
    UINT16 total_length;
    UINT16 identification;
    UINT16 flags_offset;
    UINT8 ttl;
    UINT8 protocol;
    UINT16 checksum;
    ip_addr_t src_ip;
    ip_addr_t dest_ip;
} __attribute__((packed)) ip_header_t;

typedef struct {
    UINT16 src_port;
    UINT16 dest_port;
    UINT32 seq;
    UINT32 ack;
    UINT8 data_offset;
    UINT8 flags;
    UINT16 window;
    UINT16 checksum;
    UINT16 urgent;
} __attribute__((packed)) tcp_header_t;

typedef struct {
    UINT16 src_port;
    UINT16 dest_port;
    UINT16 length;
    UINT16 checksum;
} __attribute__((packed)) udp_header_t;

typedef struct {
    UINT8 type;
    UINT8 code;
    UINT16 checksum;
    UINT32 rest;
} __attribute__((packed)) icmp_header_t;

#define ETH_TYPE_IP 0x0800
#define ETH_TYPE_ARP 0x0806

#define IP_PROTO_ICMP 1
#define IP_PROTO_TCP 6
#define IP_PROTO_UDP 17

#define TCP_FLAG_FIN 0x01
#define TCP_FLAG_SYN 0x02
#define TCP_FLAG_RST 0x04
#define TCP_FLAG_ACK 0x10

typedef enum {
    NET_STATE_DOWN,
    NET_STATE_UP
} net_state_t;

typedef struct {
    UINT8 device_index;
    eth_addr_t mac;
    ip_addr_t ip;
    ip_addr_t netmask;
    ip_addr_t gateway;
    net_state_t state;
    UINT8 packet_buffer[NET_PACKET_SIZE];
} net_interface_t;

typedef struct {
    BOOLEAN active;
    UINT16 local_port;
    ip_addr_t remote_ip;
    UINT16 remote_port;
    UINT32 seq;
    UINT32 ack;
    UINT8 state;
} tcp_connection_t;

void net_stack_init(UINT8 device_index);
BOOLEAN net_stack_poll(void);
BOOLEAN net_set_ip(const UINT8 *ip, const UINT8 *netmask, const UINT8 *gateway);
BOOLEAN net_send_udp(ip_addr_t dest_ip, UINT16 dest_port, const UINT8 *data, UINTN len);
BOOLEAN net_send_tcp(ip_addr_t dest_ip, UINT16 dest_port, const UINT8 *data, UINTN len, UINT8 flags);
UINTN net_receive_packet(UINT8 *buffer, UINTN max_size);

#endif