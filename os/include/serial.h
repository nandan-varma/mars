#ifndef SERIAL_H
#define SERIAL_H

#include "uefi.h"

// COM1 polled output. Safe to call from any context including panic and
// scheduler critical sections — never blocks indefinitely (port write goes
// straight to the QEMU serial backend; on real hardware UART FIFO drains).
//
// On real hardware, call serial_init() once during early boot to set baud
// and line control. Under QEMU defaults work without init, so callers that
// run pre-init still see output.

void serial_init(void);
void serial_write_byte(UINT8 byte);
void serial_write_str(const char *text);

// Format helpers used by diag/panic dumps. They write fixed-width
// representations so log lines stay grep-able.
void serial_write_hex64(UINT64 value);
void serial_write_hex32(UINT32 value);
void serial_write_dec(UINT64 value);

#endif
