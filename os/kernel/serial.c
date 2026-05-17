#include "serial.h"

#define COM1_PORT 0x3F8

static BOOLEAN g_serial_initialized = FALSE;

static void outb(UINT16 port, UINT8 value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static UINT8 inb(UINT16 port) {
    UINT8 value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

void serial_init(void) {
    if (g_serial_initialized) {
        return;
    }

    // 8N1 @ 115200, FIFOs enabled. Idempotent on QEMU; mandatory on real HW.
    outb(COM1_PORT + 1, 0x00);  // Disable interrupts
    outb(COM1_PORT + 3, 0x80);  // DLAB on
    outb(COM1_PORT + 0, 0x01);  // Divisor low (1 = 115200)
    outb(COM1_PORT + 1, 0x00);  // Divisor high
    outb(COM1_PORT + 3, 0x03);  // 8N1, DLAB off
    outb(COM1_PORT + 2, 0xC7);  // FIFO on, clear, 14-byte threshold
    outb(COM1_PORT + 4, 0x0B);  // RTS/DSR set
    g_serial_initialized = TRUE;
}

static BOOLEAN tx_ready(void) {
    return (inb(COM1_PORT + 5) & 0x20) != 0;
}

void serial_write_byte(UINT8 byte) {
    // Bounded polling so a wedged UART can't hang the panic path forever.
    for (UINTN i = 0; i < 100000; ++i) {
        if (tx_ready()) {
            break;
        }
    }
    outb(COM1_PORT, byte);
}

void serial_write_str(const char *text) {
    if (text == NULL) {
        return;
    }
    for (UINTN i = 0; text[i] != 0; ++i) {
        serial_write_byte((UINT8)text[i]);
    }
}

static const char k_hex_digits[] = "0123456789abcdef";

static void serial_write_hex_n(UINT64 value, UINTN nibbles) {
    for (UINTN i = nibbles; i > 0; --i) {
        UINT8 nibble = (UINT8)((value >> ((i - 1) * 4)) & 0xF);
        serial_write_byte((UINT8)k_hex_digits[nibble]);
    }
}

void serial_write_hex64(UINT64 value) {
    serial_write_hex_n(value, 16);
}

void serial_write_hex32(UINT32 value) {
    serial_write_hex_n((UINT64)value, 8);
}

void serial_write_dec(UINT64 value) {
    if (value == 0) {
        serial_write_byte('0');
        return;
    }

    char buf[24];
    UINTN len = 0;
    while (value > 0 && len < sizeof(buf)) {
        buf[len++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while (len > 0) {
        serial_write_byte((UINT8)buf[--len]);
    }
}
