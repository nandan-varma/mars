#include "pci.h"
#include "diag.h"

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

#define PCI_VENDOR_ID_INVALID 0xFFFFU

#define DIAG_DOMAIN_PCI 0x0A00U
#define DIAG_CODE_PCI_ENUMERATE_BEGIN  0x01U
#define DIAG_CODE_PCI_ENUMERATE_END    0x02U
#define DIAG_CODE_PCI_DEVICE_FOUND     0x10U

static pci_device_t g_devices[PCI_MAX_DEVICES];
static UINTN g_device_count;

static void outl(UINT16 port, UINT32 value) {
    __asm__ volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}

static UINT32 inl(UINT16 port) {
    UINT32 value;
    __asm__ volatile("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static UINT32 config_address(UINT8 bus, UINT8 device, UINT8 function, UINT8 offset) {
    return (UINT32)0x80000000
         | ((UINT32)bus      << 16)
         | ((UINT32)device   << 11)
         | ((UINT32)function << 8)
         | ((UINT32)offset   & 0xFC);
}

UINT32 pci_config_read32(UINT8 bus, UINT8 device, UINT8 function, UINT8 offset) {
    outl(PCI_CONFIG_ADDRESS, config_address(bus, device, function, offset));
    return inl(PCI_CONFIG_DATA);
}

UINT16 pci_config_read16(UINT8 bus, UINT8 device, UINT8 function, UINT8 offset) {
    UINT32 value = pci_config_read32(bus, device, function, offset & 0xFC);
    return (UINT16)((value >> ((offset & 2) * 8)) & 0xFFFF);
}

UINT8 pci_config_read8(UINT8 bus, UINT8 device, UINT8 function, UINT8 offset) {
    UINT32 value = pci_config_read32(bus, device, function, offset & 0xFC);
    return (UINT8)((value >> ((offset & 3) * 8)) & 0xFF);
}

void pci_config_write32(UINT8 bus, UINT8 device, UINT8 function, UINT8 offset, UINT32 value) {
    outl(PCI_CONFIG_ADDRESS, config_address(bus, device, function, offset));
    outl(PCI_CONFIG_DATA, value);
}

static BOOLEAN probe_function(UINT8 bus, UINT8 device, UINT8 function, pci_device_t *out) {
    UINT16 vendor = pci_config_read16(bus, device, function, 0x00);
    if (vendor == PCI_VENDOR_ID_INVALID) {
        return FALSE;
    }

    out->bus = bus;
    out->device = device;
    out->function = function;
    out->vendor_id = vendor;
    out->device_id = pci_config_read16(bus, device, function, 0x02);

    UINT32 class_reg = pci_config_read32(bus, device, function, 0x08);
    out->revision   = (UINT8)(class_reg & 0xFF);
    out->prog_if    = (UINT8)((class_reg >> 8) & 0xFF);
    out->subclass   = (UINT8)((class_reg >> 16) & 0xFF);
    out->class_code = (UINT8)((class_reg >> 24) & 0xFF);

    out->header_type = pci_config_read8(bus, device, function, 0x0E);

    // Only header type 0 (normal device) has BARs at 0x10-0x24. Type 1
    // (PCI-to-PCI bridge) has BARs at 0x10-0x14; type 2 (CardBus) is
    // different again. We're conservative: only read BARs for type 0.
    for (UINTN i = 0; i < 6; ++i) {
        out->bar[i] = 0;
    }
    if ((out->header_type & 0x7F) == 0x00) {
        for (UINTN i = 0; i < 6; ++i) {
            out->bar[i] = pci_config_read32(bus, device, function, (UINT8)(0x10 + i * 4));
        }
        out->irq_line = pci_config_read8(bus, device, function, 0x3C);
        out->irq_pin  = pci_config_read8(bus, device, function, 0x3D);
    } else {
        out->irq_line = 0xFF;
        out->irq_pin = 0;
    }

    return TRUE;
}

static BOOLEAN is_multifunction(UINT8 bus, UINT8 device) {
    UINT8 header = pci_config_read8(bus, device, 0, 0x0E);
    return (header & 0x80) != 0;
}

UINTN pci_enumerate(void) {
    g_device_count = 0;
    diag_log(DIAG_DOMAIN_PCI, DIAG_CODE_PCI_ENUMERATE_BEGIN, 0, 0);

    for (UINT32 bus = 0; bus < 256 && g_device_count < PCI_MAX_DEVICES; ++bus) {
        for (UINT32 device = 0; device < 32 && g_device_count < PCI_MAX_DEVICES; ++device) {
            pci_device_t probe;
            if (!probe_function((UINT8)bus, (UINT8)device, 0, &probe)) {
                continue;
            }

            g_devices[g_device_count++] = probe;
            diag_log(DIAG_DOMAIN_PCI, DIAG_CODE_PCI_DEVICE_FOUND,
                     ((UINT64)probe.class_code << 32) | ((UINT64)probe.subclass << 24) | probe.vendor_id,
                     ((UINT64)bus << 16) | ((UINT64)device << 8) | probe.device_id);

            if (!is_multifunction((UINT8)bus, (UINT8)device)) {
                continue;
            }

            for (UINT32 function = 1; function < 8 && g_device_count < PCI_MAX_DEVICES; ++function) {
                if (probe_function((UINT8)bus, (UINT8)device, (UINT8)function, &probe)) {
                    g_devices[g_device_count++] = probe;
                    diag_log(DIAG_DOMAIN_PCI, DIAG_CODE_PCI_DEVICE_FOUND,
                             ((UINT64)probe.class_code << 32) | ((UINT64)probe.subclass << 24) | probe.vendor_id,
                             ((UINT64)bus << 16) | ((UINT64)device << 8) | probe.device_id);
                }
            }
        }
    }

    diag_log(DIAG_DOMAIN_PCI, DIAG_CODE_PCI_ENUMERATE_END, g_device_count, 0);
    return g_device_count;
}

UINTN pci_device_count(void) {
    return g_device_count;
}

BOOLEAN pci_get_device(UINTN index, pci_device_t *out) {
    if (out == NULL || index >= g_device_count) {
        return FALSE;
    }
    *out = g_devices[index];
    return TRUE;
}

BOOLEAN pci_find_device(UINT16 vendor_id, UINT16 device_id, pci_device_t *out) {
    if (out == NULL) {
        return FALSE;
    }
    for (UINTN i = 0; i < g_device_count; ++i) {
        const pci_device_t *d = &g_devices[i];
        if ((vendor_id == 0xFFFF || d->vendor_id == vendor_id) &&
            (device_id == 0xFFFF || d->device_id == device_id)) {
            *out = *d;
            return TRUE;
        }
    }
    return FALSE;
}

BOOLEAN pci_find_class(UINT8 class_code, UINT8 subclass, pci_device_t *out) {
    if (out == NULL) {
        return FALSE;
    }
    for (UINTN i = 0; i < g_device_count; ++i) {
        const pci_device_t *d = &g_devices[i];
        if (d->class_code == class_code && d->subclass == subclass) {
            *out = *d;
            return TRUE;
        }
    }
    return FALSE;
}
