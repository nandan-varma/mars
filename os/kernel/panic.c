#include "panic.h"

#include "diag.h"
#include "serial.h"

static panic_fault_t g_last_fault;

static void dump_fault(const panic_fault_t *fault) {
    serial_write_str("\r\n*** MARS PANIC ***\r\n");
    serial_write_str("  vector=0x");
    serial_write_hex64((UINT64)fault->vector);
    serial_write_str("  code=0x");
    serial_write_hex64(fault->code);
    serial_write_str("  address=0x");
    serial_write_hex64(fault->address);
    serial_write_str("\r\n  stage=0x");
    serial_write_hex32(diag_stage());
    serial_write_str("\r\n");
}

static void halt_forever(void) {
    serial_write_str("*** halted ***\r\n");
    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}

void panic_trap(UINTN vector, UINT64 code, UINT64 address) {
    g_last_fault.vector = vector;
    g_last_fault.code = code;
    g_last_fault.address = address;
    diag_capture_crash(vector, code, address);

    dump_fault(&g_last_fault);
    diag_dump_stage_history_to_serial();
    diag_dump_records_to_serial(64);
    halt_forever();
}

void panic_now(UINT64 code) {
    panic_trap(0xFFFFU, code, 0);
}

panic_fault_t panic_last_fault(void) {
    return g_last_fault;
}
