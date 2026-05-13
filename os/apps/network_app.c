#include "uefi.h"
#include "app.h"
#include "input.h"
#include "process.h"
#include "scheduler.h"
#include "heap.h"
#include "network_stack.h"
#include "net_driver.h"
#include "wm.h"
#include "diag.h"
#include "app_internal.h"

typedef struct {
    CHAR16 content[512];
    UINT32 content_len;
    BOOLEAN network_present;
    UINT8 mac_addr[6];
    UINT8 ip_addr[4];
    UINTN tick_count;
} network_app_state_t;

static network_app_state_t *g_net_state = NULL;

static void update_network_info(void) {
    if (g_net_state == NULL) {
        return;
    }

    UINTN idx = 0;

    g_net_state->content[idx++] = L'N';
    g_net_state->content[idx++] = L'e';
    g_net_state->content[idx++] = L't';
    g_net_state->content[idx++] = L'w';
    g_net_state->content[idx++] = L'o';
    g_net_state->content[idx++] = L'r';
    g_net_state->content[idx++] = L'k';
    g_net_state->content[idx++] = L' ';
    g_net_state->content[idx++] = L'S';
    g_net_state->content[idx++] = L't';
    g_net_state->content[idx++] = L'a';
    g_net_state->content[idx++] = L't';
    g_net_state->content[idx++] = L'u';
    g_net_state->content[idx++] = L's';
    g_net_state->content[idx++] = L':';
    g_net_state->content[idx++] = L'\n';
    g_net_state->content[idx++] = L'\n';

    if (!g_net_state->network_present) {
        const CHAR16 *msg = L"No NIC detected\n\nUse QEMU with:\n-netdev user,device e1000";
        for (UINTN i = 0; msg[i] != 0 && idx < 500; ++i) {
            g_net_state->content[idx++] = msg[i];
        }
    } else {
        const CHAR16 *connected = L"Status: Connected\n";
        for (UINTN i = 0; connected[i] != 0 && idx < 500; ++i) {
            g_net_state->content[idx++] = connected[i];
        }

        const CHAR16 *mac_str = L"MAC: ";
        for (UINTN i = 0; mac_str[i] != 0 && idx < 500; ++i) {
            g_net_state->content[idx++] = mac_str[i];
        }
        for (UINTN i = 0; i < 6; ++i) {
            UINT8 nibble_high = (g_net_state->mac_addr[i] >> 4) & 0x0F;
            UINT8 nibble_low = g_net_state->mac_addr[i] & 0x0F;
            g_net_state->content[idx++] = nibble_high < 10 ? (L'0' + nibble_high) : (L'A' + nibble_high - 10);
            g_net_state->content[idx++] = nibble_low < 10 ? (L'0' + nibble_low) : (L'A' + nibble_low - 10);
            if (i < 5) g_net_state->content[idx++] = L':';
        }
        g_net_state->content[idx++] = L'\n';

        const CHAR16 *ip_label = L"IP: ";
        for (UINTN i = 0; ip_label[i] != 0 && idx < 500; ++i) {
            g_net_state->content[idx++] = ip_label[i];
        }
        for (UINTN i = 0; i < 4; ++i) {
            UINT8 octet = g_net_state->ip_addr[i];
            CHAR16 octet_buf[4];
            UINTN octet_idx = 0;
            if (octet == 0) {
                octet_buf[octet_idx++] = L'0';
            } else {
                CHAR16 rev[4];
                UINTN rev_idx = 0;
                while (octet > 0) {
                    rev[rev_idx++] = L'0' + (octet % 10);
                    octet /= 10;
                }
                while (rev_idx > 0) {
                    octet_buf[octet_idx++] = rev[--rev_idx];
                }
            }
            octet_buf[octet_idx] = 0;
            for (UINTN j = 0; octet_buf[j] != 0 && idx < 500; ++j) {
                g_net_state->content[idx++] = octet_buf[j];
            }
            if (i < 3) g_net_state->content[idx++] = L'.';
        }
        g_net_state->content[idx++] = L'\n';

        const CHAR16 *proto = L"\nProtocols: IP/ICMP/UDP/TCP\n";
        for (UINTN i = 0; proto[i] != 0 && idx < 500; ++i) {
            g_net_state->content[idx++] = proto[i];
        }

        const CHAR16 *hint = L"Press T to test network";
        for (UINTN i = 0; hint[i] != 0 && idx < 500; ++i) {
            g_net_state->content[idx++] = hint[i];
        }
    }

    g_net_state->content_len = idx;
}

BOOLEAN network_app_init(UINT32 window_id) {
    (void)window_id;
    g_net_state = heap_alloc(sizeof(network_app_state_t));
    if (g_net_state == NULL) {
        return FALSE;
    }

    g_net_state->content[0] = 0;
    g_net_state->content_len = 0;
    g_net_state->network_present = FALSE;
    g_net_state->tick_count = 0;

    for (UINTN i = 0; i < 6; ++i) {
        g_net_state->mac_addr[i] = 0;
    }
    for (UINTN i = 0; i < 4; ++i) {
        g_net_state->ip_addr[i] = 0;
    }

    net_driver_init();
    if (net_driver_discover()) {
        g_net_state->network_present = TRUE;
        net_driver_get_mac_address(0, g_net_state->mac_addr);

        net_stack_init(0);
        UINT8 default_ip[4] = { 192, 168, 1, 100 };
        UINT8 default_mask[4] = { 255, 255, 255, 0 };
        UINT8 default_gw[4] = { 192, 168, 1, 1 };
        net_set_ip(default_ip, default_mask, default_gw);

        for (UINTN i = 0; i < 4; ++i) {
            g_net_state->ip_addr[i] = default_ip[i];
        }
    }

    update_network_info();

    return TRUE;
}

void network_app_render(app_instance_t *instance) {
    if (g_net_state == NULL) {
        return;
    }

    g_net_state->tick_count++;

    if (g_net_state->tick_count % 60 == 0) {
        update_network_info();
    }

    if (g_net_state->network_present) {
        net_stack_poll();
    }

    // Copy content to instance
    UINTN len = g_net_state->content_len;
    if (len > APP_CONTENT_CHARS) len = APP_CONTENT_CHARS;
    for (UINTN i = 0; i < len; ++i) {
        instance->content[i] = g_net_state->content[i];
    }
    instance->content[len] = 0;
    instance->content_len = (UINT32)len;
}

void network_app_handle_input(const input_event_t *event) {
    if (g_net_state == NULL || event == NULL) {
        return;
    }

    if (event->type == INPUT_EVENT_KEY_DOWN) {
        if (event->data.key.unicode == L't' || event->data.key.unicode == L'T') {
            if (g_net_state->network_present) {
                UINT8 test_ip[4] = { 8, 8, 8, 8 };
                ip_addr_t dest_ip;
                for (UINTN i = 0; i < 4; ++i) {
                    dest_ip.addr[i] = test_ip[i];
                }
                UINT8 ping_data[16] = "PING";
                net_send_udp(dest_ip, 53, ping_data, 4);
                diag_log(0x830U, 1, 0, 0);
            }
        }
    }
}

extern void network_app_register(void) {
    app_manifest_t manifest = {
        L"network_app",
        L"Network Settings",
        CAP_INPUT | CAP_SYSTEM,
        450, 150,
        400, 400
    };
    app_register(&manifest);
}