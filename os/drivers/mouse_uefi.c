#include "mouse_uefi.h"

#define MAX_POINTER_PROTOCOLS 8

static EFI_BOOT_SERVICES *g_boot_services;
static EFI_SIMPLE_POINTER_PROTOCOL *g_simple_protocols[MAX_POINTER_PROTOCOLS];
static EFI_ABSOLUTE_POINTER_PROTOCOL *g_absolute_protocols[MAX_POINTER_PROTOCOLS];
static UINTN g_simple_count;
static UINTN g_absolute_count;
static BOOLEAN g_ps2_enabled;
static UINT8 g_ps2_packet[3];
static UINTN g_ps2_packet_index;
static INT32 g_mouse_x;
static INT32 g_mouse_y;
static INT32 g_screen_w;
static INT32 g_screen_h;
static BOOLEAN g_left_down;

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

static UINT8 io_in8(UINT16 port) {
    UINT8 value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void io_out8(UINT16 port, UINT8 value) {
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
    if (!ps2_wait_input_clear(100000)) {
        return FALSE;
    }
    io_out8(0x64, 0xD4);

    if (!ps2_wait_input_clear(100000)) {
        return FALSE;
    }
    io_out8(0x60, value);

    if (!ps2_wait_output_full(100000)) {
        return FALSE;
    }

    return io_in8(0x60) == 0xFA;
}

static BOOLEAN ps2_mouse_init(void) {
    if (!ps2_wait_input_clear(100000)) {
        return FALSE;
    }
    io_out8(0x64, 0xA8);

    if (!ps2_wait_input_clear(100000)) {
        return FALSE;
    }
    io_out8(0x64, 0x20);
    if (!ps2_wait_output_full(100000)) {
        return FALSE;
    }

    UINT8 command_byte = io_in8(0x60);
    command_byte |= 0x02;
    command_byte &= (UINT8)~0x20;

    if (!ps2_wait_input_clear(100000)) {
        return FALSE;
    }
    io_out8(0x64, 0x60);
    if (!ps2_wait_input_clear(100000)) {
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

static INT32 clamp(INT32 value, INT32 min_value, INT32 max_value) {
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

static UINTN poll_simple_pointer(input_event_t *events_out, UINTN max_events) {
    if (g_simple_count == 0 || events_out == NULL || max_events == 0) {
        return 0;
    }

    for (UINTN protocol_index = 0; protocol_index < g_simple_count; ++protocol_index) {
        EFI_SIMPLE_POINTER_PROTOCOL *protocol = g_simple_protocols[protocol_index];
        EFI_SIMPLE_POINTER_STATE state;
        EFI_STATUS status = protocol->GetState(protocol, &state);
        if (status == EFI_NOT_READY || EFI_ERROR(status)) {
            continue;
        }

        UINTN event_count = 0;

        INT32 dx = state.RelativeMovementX;
        INT32 dy = state.RelativeMovementY;

        if (dx > 16 || dx < -16) {
            dx /= 4;
        }
        if (dy > 16 || dy < -16) {
            dy /= 4;
        }

        if (dx != 0 || dy != 0) {
            g_mouse_x = clamp(g_mouse_x + dx, 0, g_screen_w - 1);
            g_mouse_y = clamp(g_mouse_y + dy, 0, g_screen_h - 1);

            events_out[event_count].type = INPUT_EVENT_MOUSE_MOVE;
            events_out[event_count].data.mouse_move.dx = dx;
            events_out[event_count].data.mouse_move.dy = dy;
            events_out[event_count].data.mouse_move.x = g_mouse_x;
            events_out[event_count].data.mouse_move.y = g_mouse_y;
            ++event_count;
        }

        BOOLEAN left_now = state.LeftButton ? TRUE : FALSE;
        if (left_now != g_left_down && event_count < max_events) {
            events_out[event_count].type = left_now ? INPUT_EVENT_MOUSE_BUTTON_DOWN : INPUT_EVENT_MOUSE_BUTTON_UP;
            events_out[event_count].data.mouse_button.left = left_now;
            events_out[event_count].data.mouse_button.right = state.RightButton ? TRUE : FALSE;
            ++event_count;
            g_left_down = left_now;
        }

        if (event_count > 0) {
            return event_count;
        }
    }

    return 0;
}

static UINTN poll_absolute_pointer(input_event_t *events_out, UINTN max_events) {
    if (g_absolute_count == 0 || events_out == NULL || max_events == 0) {
        return 0;
    }

    for (UINTN protocol_index = 0; protocol_index < g_absolute_count; ++protocol_index) {
        EFI_ABSOLUTE_POINTER_PROTOCOL *protocol = g_absolute_protocols[protocol_index];
        EFI_ABSOLUTE_POINTER_STATE state;
        EFI_STATUS status = protocol->GetState(protocol, &state);
        if (status == EFI_NOT_READY || EFI_ERROR(status) || protocol->Mode == NULL) {
            continue;
        }

        UINT64 min_x = protocol->Mode->AbsoluteMinX;
        UINT64 max_x = protocol->Mode->AbsoluteMaxX;
        UINT64 min_y = protocol->Mode->AbsoluteMinY;
        UINT64 max_y = protocol->Mode->AbsoluteMaxY;

        INT32 mapped_x = g_mouse_x;
        INT32 mapped_y = g_mouse_y;

        if (max_x > min_x) {
            mapped_x = (INT32)(((state.CurrentX - min_x) * (UINT64)(g_screen_w - 1)) / (max_x - min_x));
        }
        if (max_y > min_y) {
            mapped_y = (INT32)(((state.CurrentY - min_y) * (UINT64)(g_screen_h - 1)) / (max_y - min_y));
        }

        mapped_x = clamp(mapped_x, 0, g_screen_w - 1);
        mapped_y = clamp(mapped_y, 0, g_screen_h - 1);

        UINTN event_count = 0;
        INT32 dx = mapped_x - g_mouse_x;
        INT32 dy = mapped_y - g_mouse_y;

        if ((dx != 0 || dy != 0) && event_count < max_events) {
            g_mouse_x = mapped_x;
            g_mouse_y = mapped_y;
            events_out[event_count].type = INPUT_EVENT_MOUSE_MOVE;
            events_out[event_count].data.mouse_move.dx = dx;
            events_out[event_count].data.mouse_move.dy = dy;
            events_out[event_count].data.mouse_move.x = g_mouse_x;
            events_out[event_count].data.mouse_move.y = g_mouse_y;
            ++event_count;
        }

        BOOLEAN left_now = state.ActiveButtons != 0 ? TRUE : FALSE;
        if (left_now != g_left_down && event_count < max_events) {
            events_out[event_count].type = left_now ? INPUT_EVENT_MOUSE_BUTTON_DOWN : INPUT_EVENT_MOUSE_BUTTON_UP;
            events_out[event_count].data.mouse_button.left = left_now;
            events_out[event_count].data.mouse_button.right = FALSE;
            ++event_count;
            g_left_down = left_now;
        }

        if (event_count > 0) {
            return event_count;
        }
    }

    return 0;
}

static UINTN poll_ps2_mouse(input_event_t *events_out, UINTN max_events) {
    if (!g_ps2_enabled || events_out == NULL || max_events == 0) {
        return 0;
    }

    UINTN event_count = 0;
    UINTN bytes_budget = 64;

    while (bytes_budget-- > 0 && (io_in8(0x64) & 0x01)) {
        UINT8 status = io_in8(0x64);
        UINT8 data = io_in8(0x60);

        if ((status & 0x20) == 0) {
            continue;
        }

        if (g_ps2_packet_index == 0 && (data & 0x08) == 0) {
            continue;
        }

        g_ps2_packet[g_ps2_packet_index++] = data;
        if (g_ps2_packet_index < 3) {
            continue;
        }

        g_ps2_packet_index = 0;

        INT32 dx = (INT8)g_ps2_packet[1];
        INT32 dy = -(INT8)g_ps2_packet[2];

        if (dx != 0 || dy != 0) {
            g_mouse_x = clamp(g_mouse_x + dx, 0, g_screen_w - 1);
            g_mouse_y = clamp(g_mouse_y + dy, 0, g_screen_h - 1);
            if (event_count < max_events) {
                events_out[event_count].type = INPUT_EVENT_MOUSE_MOVE;
                events_out[event_count].data.mouse_move.dx = dx;
                events_out[event_count].data.mouse_move.dy = dy;
                events_out[event_count].data.mouse_move.x = g_mouse_x;
                events_out[event_count].data.mouse_move.y = g_mouse_y;
                ++event_count;
            }
        }

        BOOLEAN left_now = (g_ps2_packet[0] & 0x01) ? TRUE : FALSE;
        if (left_now != g_left_down && event_count < max_events) {
            events_out[event_count].type = left_now ? INPUT_EVENT_MOUSE_BUTTON_DOWN : INPUT_EVENT_MOUSE_BUTTON_UP;
            events_out[event_count].data.mouse_button.left = left_now;
            events_out[event_count].data.mouse_button.right = (g_ps2_packet[0] & 0x02) ? TRUE : FALSE;
            ++event_count;
            g_left_down = left_now;
        }

        if (event_count >= max_events) {
            break;
        }
    }

    return event_count;
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
