#ifndef MOUSE_UEFI_H
#define MOUSE_UEFI_H

#include "uefi.h"
#include "input.h"

void mouse_driver_init(
	EFI_BOOT_SERVICES *boot_services,
	EFI_SIMPLE_POINTER_PROTOCOL *simple_protocol,
	EFI_ABSOLUTE_POINTER_PROTOCOL *absolute_protocol,
	UINT32 screen_w,
	UINT32 screen_h
);
UINTN mouse_driver_poll(input_event_t *events_out, UINTN max_events);
INT32 mouse_driver_x(void);
INT32 mouse_driver_y(void);
BOOLEAN mouse_driver_left_down(void);
BOOLEAN mouse_driver_has_simple(void);
BOOLEAN mouse_driver_has_absolute(void);
BOOLEAN mouse_driver_has_ps2(void);

#endif
