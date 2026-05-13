#ifndef MOUSE_INTERNAL_H
#define MOUSE_INTERNAL_H

#include "mouse_uefi.h"

extern EFI_BOOT_SERVICES *g_boot_services;
extern EFI_SIMPLE_POINTER_PROTOCOL *g_simple_protocols[];
extern EFI_ABSOLUTE_POINTER_PROTOCOL *g_absolute_protocols[];
extern UINTN g_simple_count;
extern UINTN g_absolute_count;
extern BOOLEAN g_ps2_enabled;
extern UINT8 g_ps2_packet[3];
extern UINTN g_ps2_packet_index;
extern INT32 g_mouse_x;
extern INT32 g_mouse_y;
extern INT32 g_screen_w;
extern INT32 g_screen_h;
extern BOOLEAN g_left_down;

UINT8 io_in8(UINT16 port);
void io_out8(UINT16 port, UINT8 value);
INT32 clamp(INT32 value, INT32 min_value, INT32 max_value);

UINTN poll_simple_pointer(input_event_t *events_out, UINTN max_events);
UINTN poll_absolute_pointer(input_event_t *events_out, UINTN max_events);
UINTN poll_ps2_mouse(input_event_t *events_out, UINTN max_events);

#endif
