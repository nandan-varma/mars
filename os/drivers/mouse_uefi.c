#include "mouse_uefi.h"
#include "mouse_internal.h"

// SECURITY FIX (HIGH #8): Reduce unbounded polling timeout from 100000 to 1000
// to prevent potential DoS via stuck hardware. This maintains responsiveness
// while avoiding infinite waits. CWE-835: Infinite Loop
#define MAX_POINTER_PROTOCOLS 8
#define PS2_POLL_TIMEOUT 1000

EFI_BOOT_SERVICES *g_boot_services;
EFI_SIMPLE_POINTER_PROTOCOL *g_simple_protocols[MAX_POINTER_PROTOCOLS];
EFI_ABSOLUTE_POINTER_PROTOCOL *g_absolute_protocols[MAX_POINTER_PROTOCOLS];
UINTN g_simple_count;
UINTN g_absolute_count;
BOOLEAN g_ps2_enabled;
UINT8 g_ps2_packet[3];
UINTN g_ps2_packet_index;
INT32 g_mouse_x;
INT32 g_mouse_y;
INT32 g_screen_w;
INT32 g_screen_h;
BOOLEAN g_left_down;

static void clear_protocol_lists(void) {
    g_simple_count = 0;
    g_absolute_count = 0;
}

static void add_simple_protocol(EFI_SIMPLE_POINTER_PROTOCOL *protocol) {
    if (protocol == NULL || protocol->Mode == NULL) {
        return;
    }

    for (UINTN i = 0; i < g_simple_count; ++i) {
        if (g_simple_protocols[i] == protocol) {
            return;
        }
    }

    if (g_simple_count < MAX_POINTER_PROTOCOLS) {
        g_simple_protocols[g_simple_count++] = protocol;
    }
}

static void add_absolute_protocol(EFI_ABSOLUTE_POINTER_PROTOCOL *protocol) {
    if (protocol == NULL || protocol->Mode == NULL) {
        return;
    }

    if (!(protocol->Mode->AbsoluteMaxX > protocol->Mode->AbsoluteMinX
        && protocol->Mode->AbsoluteMaxY > protocol->Mode->AbsoluteMinY)) {
        return;
    }

    for (UINTN i = 0; i < g_absolute_count; ++i) {
        if (g_absolute_protocols[i] == protocol) {
            return;
        }
    }

    if (g_absolute_count < MAX_POINTER_PROTOCOLS) {
        g_absolute_protocols[g_absolute_count++] = protocol;
    }
}

static void discover_simple_protocols(void) {
    if (g_boot_services == NULL) {
        return;
    }

    EFI_GUID guid = EFI_SIMPLE_POINTER_PROTOCOL_GUID;
    EFI_HANDLE *handles = NULL;
    UINTN handle_count = 0;
    EFI_STATUS status = g_boot_services->LocateHandleBuffer(ByProtocol, &guid, NULL, &handle_count, &handles);
    if (EFI_ERROR(status) || handles == NULL) {
        return;
    }

    for (UINTN i = 0; i < handle_count; ++i) {
        EFI_SIMPLE_POINTER_PROTOCOL *candidate = NULL;
        if (!EFI_ERROR(g_boot_services->HandleProtocol(handles[i], &guid, (VOID **)&candidate))) {
            add_simple_protocol(candidate);
        }
    }

    (void)g_boot_services->FreePool(handles);
}

static void discover_absolute_protocols(void) {
    if (g_boot_services == NULL) {
        return;
    }

    EFI_GUID guid = EFI_ABSOLUTE_POINTER_PROTOCOL_GUID;
    EFI_HANDLE *handles = NULL;
    UINTN handle_count = 0;
    EFI_STATUS status = g_boot_services->LocateHandleBuffer(ByProtocol, &guid, NULL, &handle_count, &handles);
    if (EFI_ERROR(status) || handles == NULL) {
        return;
    }

    for (UINTN i = 0; i < handle_count; ++i) {
        EFI_ABSOLUTE_POINTER_PROTOCOL *candidate = NULL;
        if (!EFI_ERROR(g_boot_services->HandleProtocol(handles[i], &guid, (VOID **)&candidate))) {
            add_absolute_protocol(candidate);
        }
    }

    (void)g_boot_services->FreePool(handles);
}

UINT8 io_in8(UINT16 port) {
    UINT8 value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

void io_out8(UINT16 port, UINT8 value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static BOOLEAN ps2_wait_input_clear(UINTN attempts) {
    while (attempts-- > 0) {
        if ((io_in8(0x64) & 0x02) == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

static BOOLEAN ps2_wait_output_full(UINTN attempts) {
    while (attempts-- > 0) {
        if (io_in8(0x64) & 0x01) {
            return TRUE;
        }
    }
    return FALSE;
}

static BOOLEAN ps2_write_mouse(UINT8 value) {
    if (!ps2_wait_input_clear(PS2_POLL_TIMEOUT)) {
        return FALSE;
    }
    io_out8(0x64, 0xD4);

    if (!ps2_wait_input_clear(PS2_POLL_TIMEOUT)) {
        return FALSE;
    }
    io_out8(0x60, value);

    if (!ps2_wait_output_full(PS2_POLL_TIMEOUT)) {
        return FALSE;
    }

    return io_in8(0x60) == 0xFA;
}

static BOOLEAN ps2_mouse_init(void) {
    if (!ps2_wait_input_clear(PS2_POLL_TIMEOUT)) {
        return FALSE;
    }
    io_out8(0x64, 0xA8);

    if (!ps2_wait_input_clear(PS2_POLL_TIMEOUT)) {
        return FALSE;
    }
    io_out8(0x64, 0x20);
    if (!ps2_wait_output_full(PS2_POLL_TIMEOUT)) {
        return FALSE;
    }

    UINT8 command_byte = io_in8(0x60);
    command_byte |= 0x02;
    command_byte &= (UINT8)~0x20;

    if (!ps2_wait_input_clear(PS2_POLL_TIMEOUT)) {
        return FALSE;
    }
    io_out8(0x64, 0x60);
    if (!ps2_wait_input_clear(PS2_POLL_TIMEOUT)) {
        return FALSE;
    }
    io_out8(0x60, command_byte);

    if (!ps2_write_mouse(0xF6)) {
        return FALSE;
    }
    if (!ps2_write_mouse(0xF4)) {
        return FALSE;
    }

    g_ps2_packet_index = 0;
    return TRUE;
}

INT32 clamp(INT32 value, INT32 min_value, INT32 max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

void mouse_driver_init(
    EFI_BOOT_SERVICES *boot_services,
    EFI_SIMPLE_POINTER_PROTOCOL *simple_protocol,
    EFI_ABSOLUTE_POINTER_PROTOCOL *absolute_protocol,
    UINT32 screen_w,
    UINT32 screen_h
) {
    g_boot_services = boot_services;
    g_ps2_enabled = FALSE;
    clear_protocol_lists();

    add_simple_protocol(simple_protocol);
    add_absolute_protocol(absolute_protocol);
    discover_simple_protocols();
    discover_absolute_protocols();

    g_screen_w = (INT32)screen_w;
    g_screen_h = (INT32)screen_h;
    g_mouse_x = g_screen_w / 2;
    g_mouse_y = g_screen_h / 2;
    g_left_down = FALSE;
    g_ps2_packet_index = 0;

    for (UINTN i = 0; i < g_simple_count; ++i) {
        (void)g_simple_protocols[i]->Reset(g_simple_protocols[i], FALSE);
    }

    for (UINTN i = 0; i < g_absolute_count; ++i) {
        (void)g_absolute_protocols[i]->Reset(g_absolute_protocols[i], FALSE);
    }

    g_ps2_enabled = ps2_mouse_init();
}

UINTN mouse_driver_poll(input_event_t *events_out, UINTN max_events) {
    if (events_out == NULL || max_events == 0) {
        return 0;
    }

    UINTN count = poll_ps2_mouse(events_out, max_events);
    if (count > 0) {
        return count;
    }

    count = poll_absolute_pointer(events_out, max_events);
    if (count > 0) {
        return count;
    }

    return poll_simple_pointer(events_out, max_events);
}

INT32 mouse_driver_x(void) {
    return g_mouse_x;
}

INT32 mouse_driver_y(void) {
    return g_mouse_y;
}

BOOLEAN mouse_driver_left_down(void) {
    return g_left_down;
}

BOOLEAN mouse_driver_has_simple(void) {
    return g_simple_count > 0;
}

BOOLEAN mouse_driver_has_absolute(void) {
    return g_absolute_count > 0;
}

BOOLEAN mouse_driver_has_ps2(void) {
    return g_ps2_enabled;
}

void mouse_driver_inject_move(INT32 dx, INT32 dy) {
    if (dx == 0 && dy == 0) {
        return;
    }

    g_mouse_x = clamp(g_mouse_x + dx, 0, g_screen_w - 1);
    g_mouse_y = clamp(g_mouse_y + dy, 0, g_screen_h - 1);
}

BOOLEAN mouse_driver_set_left(BOOLEAN down) {
    if (g_left_down == down) {
        return FALSE;
    }

    g_left_down = down;
    return TRUE;
}
