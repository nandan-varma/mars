#ifndef KEYBOARD_UEFI_H
#define KEYBOARD_UEFI_H

#include "uefi.h"
#include "input.h"

void keyboard_driver_init(EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *protocol);
BOOLEAN keyboard_driver_poll(input_event_t *event_out);

#endif
