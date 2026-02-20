#include "keyboard_uefi.h"

static EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *g_protocol;

void keyboard_driver_init(EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *protocol) {
    g_protocol = protocol;
    if (g_protocol != NULL) {
        (void)g_protocol->Reset(g_protocol, FALSE);
    }
}

BOOLEAN keyboard_driver_poll(input_event_t *event_out) {
    if (g_protocol == NULL || event_out == NULL) {
        return FALSE;
    }

    EFI_KEY_DATA key_data;
    EFI_STATUS status = g_protocol->ReadKeyStrokeEx(g_protocol, &key_data);
    if (status == EFI_NOT_READY || EFI_ERROR(status)) {
        return FALSE;
    }

    event_out->type = INPUT_EVENT_KEY_DOWN;
    event_out->data.key.unicode = key_data.Key.UnicodeChar;
    event_out->data.key.scan_code = key_data.Key.ScanCode;
    return TRUE;
}
