#ifndef PCI_H
#define PCI_H

#include "uefi.h"

// PCI configuration space access via I/O ports 0xCF8 / 0xCFC.
//
// A device is identified by a (bus, device, function) triple. The kernel
// owns a fixed-size table of discovered devices populated at boot by
// pci_enumerate(). Drivers in M5 (e1000, NVMe, xHCI, AHCI) consume that
// table to find their hardware.
//
// We intentionally do not implement PCI Express ECAM (memory-mapped
// config space) yet — that needs ACPI MCFG table parsing, which is M5
// late-stage. The legacy port-I/O config space is enough to find every
// device on a real machine; M5 drivers only need the class code, vendor,
// device, BAR0–5, and IRQ pin/line to operate.

#define PCI_MAX_DEVICES 64

typedef struct {
    UINT8  bus;
    UINT8  device;
    UINT8  function;
    UINT16 vendor_id;
    UINT16 device_id;
    UINT8  class_code;
    UINT8  subclass;
    UINT8  prog_if;
    UINT8  revision;
    UINT8  header_type;
    UINT8  irq_line;
    UINT8  irq_pin;
    UINT32 bar[6];
} pci_device_t;

// Enumerate all PCI buses and populate the internal device table.
// Returns the number of devices found. Safe to call multiple times — the
// table is rebuilt from scratch each call. Idempotent in QEMU; on real
// hardware the table is stable across boot.
UINTN pci_enumerate(void);

UINTN pci_device_count(void);
BOOLEAN pci_get_device(UINTN index, pci_device_t *out);

// Find the first device matching (vendor, device). Returns FALSE if none.
// vendor_id or device_id can be 0xFFFF to mean "match anything".
BOOLEAN pci_find_device(UINT16 vendor_id, UINT16 device_id, pci_device_t *out);

// Find the first device with the given class/subclass. Used by drivers
// that target a device category (e.g. any e1000-compatible NIC).
BOOLEAN pci_find_class(UINT8 class_code, UINT8 subclass, pci_device_t *out);

// Raw config space reads — exposed for drivers that need fields outside
// the standard header. Use the typed accessors above when possible.
UINT32 pci_config_read32(UINT8 bus, UINT8 device, UINT8 function, UINT8 offset);
UINT16 pci_config_read16(UINT8 bus, UINT8 device, UINT8 function, UINT8 offset);
UINT8  pci_config_read8(UINT8 bus, UINT8 device, UINT8 function, UINT8 offset);
void   pci_config_write32(UINT8 bus, UINT8 device, UINT8 function, UINT8 offset, UINT32 value);

#endif
