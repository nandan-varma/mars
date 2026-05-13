#include "network_stack.h"
#include "net_driver.h"
#include "diag.h"
#include "heap.h"

#define NET_ARP_CACHE_SIZE 4

static net_interface_t g_iface;
static tcp_connection_t g_tcp_connections[NET_TCP_MAX_PORTS];
static UINT8 g_arp_cache[NET_ARP_CACHE_SIZE][NET_IP_ADDR_LEN + NET_ETH_ADDR_LEN];
static UINTN g_arp_cache_count;

static eth_addr_t g_broadcast = { { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF } };

static UINT16 net_checksum(void *data, UINTN len) {
    UINT32 sum = 0;
    UINT16 *ptr = (UINT16 *)data;

    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }

    if (len == 1) {
        sum += *(UINT8 *)ptr;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (UINT16)(~sum);
}

static void net_send_ip(ip_addr_t *dest_ip, UINT8 proto, const UINT8 *data, UINTN size) {
    ip_header_t *ip = (ip_header_t *)(g_iface.packet_buffer + sizeof(eth_header_t));

    ip->version_ihl = 0x45;
    ip->tos = 0;
    ip->total_length = sizeof(ip_header_t) + size;
    ip->identification = 0;
    ip->flags_offset = 0x4000;
    ip->ttl = 64;
    ip->protocol = proto;
    ip->checksum = 0;

    for (UINTN i = 0; i < NET_IP_ADDR_LEN; ++i) {
        ip->src_ip.addr[i] = g_iface.ip.addr[i];
        ip->dest_ip.addr[i] = dest_ip->addr[i];
    }

    ip->checksum = net_checksum(ip, sizeof(ip_header_t));

    for (UINTN i = 0; i < size && (sizeof(ip_header_t) + i) < (NET_PACKET_SIZE - sizeof(eth_header_t)); ++i) {
        g_iface.packet_buffer[sizeof(eth_header_t) + sizeof(ip_header_t) + i] = data[i];
    }

    eth_header_t *eth = (eth_header_t *)g_iface.packet_buffer;
    eth->type = ETH_TYPE_IP;

    for (UINTN i = 0; i < NET_ETH_ADDR_LEN; ++i) {
        eth->dest[i] = g_broadcast.addr[i];
        eth->src[i] = g_iface.mac.addr[i];
    }

    net_driver_transmit(g_iface.device_index, g_iface.packet_buffer, sizeof(eth_header_t) + sizeof(ip_header_t) + size);
}

static void net_process_arp(const UINT8 *data, UINTN size) {
    (void)size;
    const UINT8 *arp = data + sizeof(eth_header_t);

    UINT16 opcode = *(UINT16 *)(arp + 6);

    if (opcode == 0x0100) {
        for (UINTN i = 0; i < NET_ETH_ADDR_LEN; ++i) {
            ((UINT8 *)arp)[22 + i] = g_iface.mac.addr[i];
        }
        for (UINTN i = 0; i < NET_IP_ADDR_LEN; ++i) {
            ((UINT8 *)arp)[28 + i] = g_iface.ip.addr[i];
        }

        eth_header_t *eth = (eth_header_t *)g_iface.packet_buffer;
        for (UINTN i = 0; i < NET_ETH_ADDR_LEN; ++i) {
            eth->dest[i] = arp[8 + i];
            eth->src[i] = g_iface.mac.addr[i];
        }
        eth->type = ETH_TYPE_ARP;

        *(UINT16 *)(arp + 6) = 0x0200;

        net_driver_transmit(g_iface.device_index, g_iface.packet_buffer, sizeof(eth_header_t) + 28);
    } else if (opcode == 0x0200) {
        if (g_arp_cache_count < NET_ARP_CACHE_SIZE) {
            for (UINTN i = 0; i < NET_IP_ADDR_LEN; ++i) {
                g_arp_cache[g_arp_cache_count][i] = arp[14 + i];
            }
            for (UINTN i = 0; i < NET_ETH_ADDR_LEN; ++i) {
                g_arp_cache[g_arp_cache_count][NET_IP_ADDR_LEN + i] = arp[8 + i];
            }
            g_arp_cache_count++;
        }
    }
}

static void net_process_icmp(const UINT8 *data, UINTN size) {
    (void)size;
    const ip_header_t *ip = (const ip_header_t *)(data + sizeof(eth_header_t));
    const icmp_header_t *icmp = (const icmp_header_t *)((const UINT8 *)ip + sizeof(ip_header_t));

    if (icmp->type == 8) {
        UINT8 reply_data[sizeof(icmp_header_t) + 64];
        icmp_header_t *reply = (icmp_header_t *)reply_data;

        reply->type = 0;
        reply->code = 0;
        reply->checksum = 0;
        reply->rest = icmp->rest;

        UINTN data_len = size - sizeof(ip_header_t) - sizeof(icmp_header_t);
        if (data_len > 56) data_len = 56;
        for (UINTN i = 0; i < data_len; ++i) {
            reply_data[sizeof(icmp_header_t) + i] = ((const UINT8 *)icmp)[sizeof(icmp_header_t) + i];
        }

        reply->checksum = net_checksum(reply_data, sizeof(icmp_header_t) + data_len);

        net_send_ip((ip_addr_t *)&ip->src_ip, IP_PROTO_ICMP, reply_data, sizeof(icmp_header_t) + data_len);
    }
}

static void net_process_udp(const UINT8 *data, UINTN size) {
    const ip_header_t *ip = (const ip_header_t *)(data + sizeof(eth_header_t));
    const udp_header_t *udp = (const udp_header_t *)((const UINT8 *)ip + sizeof(ip_header_t));

    UINTN payload_size = size - sizeof(eth_header_t) - sizeof(ip_header_t) - sizeof(udp_header_t);
    if (payload_size > 0 && payload_size < 512) {
        diag_log(0x820U, udp->dest_port, udp->src_port, payload_size);
    }
}

static void net_process_tcp(const UINT8 *data, UINTN size) {
    const ip_header_t *ip = (const ip_header_t *)(data + sizeof(eth_header_t));
    const tcp_header_t *tcp = (const tcp_header_t *)((const UINT8 *)ip + sizeof(ip_header_t));

    UINTN payload_size = size - sizeof(eth_header_t) - sizeof(ip_header_t) - sizeof(tcp_header_t);

    if (tcp->flags & TCP_FLAG_SYN) {
        diag_log(0x821U, tcp->dest_port, tcp->src_port, 1);
    } else if (tcp->flags & TCP_FLAG_ACK) {
        diag_log(0x821U, tcp->dest_port, tcp->src_port, payload_size);
    }
}

void net_stack_init(UINT8 device_index) {
    g_iface.device_index = device_index;
    g_iface.state = NET_STATE_DOWN;

    for (UINTN i = 0; i < NET_ETH_ADDR_LEN; ++i) {
        g_iface.mac.addr[i] = 0;
    }
    for (UINTN i = 0; i < NET_IP_ADDR_LEN; ++i) {
        g_iface.ip.addr[i] = 0;
        g_iface.netmask.addr[i] = 0;
        g_iface.gateway.addr[i] = 0;
    }

    for (UINTN i = 0; i < NET_TCP_MAX_PORTS; ++i) {
        g_tcp_connections[i].active = FALSE;
    }

    g_arp_cache_count = 0;

    diag_log(0x810U, device_index, 0, 0);
}

BOOLEAN net_set_ip(const UINT8 *ip, const UINT8 *netmask, const UINT8 *gateway) {
    if (ip == NULL || netmask == NULL || gateway == NULL) {
        return FALSE;
    }

    for (UINTN i = 0; i < NET_IP_ADDR_LEN; ++i) {
        g_iface.ip.addr[i] = ip[i];
        g_iface.netmask.addr[i] = netmask[i];
        g_iface.gateway.addr[i] = gateway[i];
    }

    g_iface.state = NET_STATE_UP;

    diag_log(0x811U, ip[0], ip[1], ip[2]);
    return TRUE;
}

BOOLEAN net_send_udp(ip_addr_t dest_ip, UINT16 dest_port, const UINT8 *data, UINTN len) {
    if (g_iface.state != NET_STATE_UP || len == 0 || len > 1400) {
        return FALSE;
    }

    udp_header_t *udp = (udp_header_t *)(g_iface.packet_buffer + sizeof(eth_header_t) + sizeof(ip_header_t));

    udp->src_port = 0x4444;
    udp->dest_port = dest_port;
    udp->length = sizeof(udp_header_t) + len;
    udp->checksum = 0;

    for (UINTN i = 0; i < len; ++i) {
        ((UINT8 *)udp)[sizeof(udp_header_t) + i] = data[i];
    }

    ip_header_t *ip = (ip_header_t *)(g_iface.packet_buffer + sizeof(eth_header_t));
    ip->src_ip = g_iface.ip;
    ip->dest_ip = dest_ip;

    net_send_ip(&dest_ip, IP_PROTO_UDP, (UINT8 *)udp, sizeof(udp_header_t) + len);

    return TRUE;
}

BOOLEAN net_send_tcp(ip_addr_t dest_ip, UINT16 dest_port, const UINT8 *data, UINTN len, UINT8 flags) {
    if (g_iface.state != NET_STATE_UP) {
        return FALSE;
    }

    tcp_header_t *tcp = (tcp_header_t *)(g_iface.packet_buffer + sizeof(eth_header_t) + sizeof(ip_header_t));

    tcp->src_port = 0x4444;
    tcp->dest_port = dest_port;
    tcp->seq = 0;
    tcp->ack = 0;
    tcp->data_offset = (5 << 4);
    tcp->flags = flags;
    tcp->window = 14600;
    tcp->checksum = 0;
    tcp->urgent = 0;

    UINTN payload_size = len < 512 ? len : 512;
    for (UINTN i = 0; i < payload_size; ++i) {
        ((UINT8 *)tcp)[sizeof(tcp_header_t) + i] = data[i];
    }

    ip_header_t *ip = (ip_header_t *)(g_iface.packet_buffer + sizeof(eth_header_t));
    ip->src_ip = g_iface.ip;
    ip->dest_ip = dest_ip;

    net_send_ip(&dest_ip, IP_PROTO_TCP, (UINT8 *)tcp, sizeof(tcp_header_t) + payload_size);

    return TRUE;
}

UINTN net_driver_receive_packet(UINT8 *buffer, UINTN max_size) {
    if (buffer == NULL || max_size == 0) {
        return 0;
    }

    UINTN size = net_driver_receive(g_iface.device_index, buffer, max_size);
    if (size == 0) {
        return 0;
    }

    eth_header_t *eth = (eth_header_t *)buffer;

    if (eth->type == ETH_TYPE_ARP) {
        net_process_arp(buffer, size);
    } else if (eth->type == ETH_TYPE_IP) {
        ip_header_t *ip = (ip_header_t *)(buffer + sizeof(eth_header_t));

        for (UINTN i = 0; i < NET_IP_ADDR_LEN; ++i) {
            if (ip->dest_ip.addr[i] != g_iface.ip.addr[i] && ip->dest_ip.addr[i] != 0xFF) {
                return 0;
            }
        }

        if (ip->protocol == IP_PROTO_ICMP) {
            net_process_icmp(buffer, size);
        } else if (ip->protocol == IP_PROTO_UDP) {
            net_process_udp(buffer, size);
        } else if (ip->protocol == IP_PROTO_TCP) {
            net_process_tcp(buffer, size);
        }
    }

    return size;
}

BOOLEAN net_stack_poll(void) {
    if (g_iface.state != NET_STATE_UP) {
        return FALSE;
    }

    UINTN size = net_driver_receive_packet(g_iface.packet_buffer, NET_PACKET_SIZE);
    return size > 0;
}