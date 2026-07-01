#include "virtio_common.hpp"
#include "pci.hpp"
#include <efi.h>
#include <efilib.h>

bool virtio_common::probe_device(virtio_common::DeviceType type, virtio_common::DeviceHandle* out) {
    const uint16_t virtio_vendor = 0x1AF4;
    for (uint8_t bus = 0; bus < 8; ++bus) {
        for (uint8_t slot = 0; slot < 32; ++slot) {
            for (uint8_t func = 0; func < 8; ++func) {
                uint16_t vendor = pci_cfg_read16(bus, slot, func, 0);
                if (vendor == 0xFFFF) continue;
                if (vendor != virtio_vendor) continue;
                uint16_t device = pci_cfg_read16(bus, slot, func, 2);
                // Match device id with requested virtio device type (legacy mapping)
                if ((uint32_t)device != (uint32_t)type) continue;
                // Read BAR0 raw value to determine IO vs MMIO and base address
                uint32_t bar0_raw = pci_cfg_read32(bus, slot, func, 0x10);
                bool is_io = (bar0_raw & 0x1u) != 0;
                uint64_t bar_addr = 0;
                if (is_io) {
                    // I/O BAR: mask lowest 2 bits
                    bar_addr = (uint64_t)(bar0_raw & ~0x3u);
                } else {
                    // Memory BAR: mask lowest 4 bits
                    // If 64-bit BAR, read next dword too
                    uint32_t bar0_low = bar0_raw & ~0xFu;
                    uint32_t bar0_high = 0;
                    // check if BAR is 64-bit type in header (bits 3..1)
                    uint32_t type = (bar0_raw >> 1) & 0x3;
                    if (type == 0x2) {
                        // 64-bit BAR, read high dword
                        bar0_high = pci_cfg_read32(bus, slot, func, 0x14);
                        bar_addr = ((uint64_t)bar0_high << 32) | (uint64_t)bar0_low;
                    } else {
                        bar_addr = (uint64_t)bar0_low;
                    }
                }

                if (out) {
                    out->bus = bus; out->slot = slot; out->func = func;
                    out->device_id = device; out->vendor_id = vendor;
                    out->bar0 = bar_addr; out->mmio = !is_io;
                }

                CHAR16 buf[256];
                UnicodeSPrint(buf, sizeof(buf), L"virtio_common: found device type=%d at %d:%d.%d vendor=0x%04x device=0x%04x BAR0=0x%lx (%s)\n",
                             (int)type, bus, slot, func, vendor, device, bar_addr, (!is_io)?L"MMIO":L"IO");
                Print(buf);
                return true;
            }
        }
    }
    return false;
}

bool virtio_common::device_init(virtio_common::DeviceHandle* h) {
    if (!h) return false;
    CHAR16 buf[256];
    UnicodeSPrint(buf, sizeof(buf), L"virtio_common: initializing device id=0x%08x at %d:%d.%d BAR0=0x%lx mmio=%d\n",
                 h->device_id, h->bus, h->slot, h->func, h->bar0, h->mmio);
    Print(buf);

    // NOTE: full feature negotiation and status register writes are device-specific and
    // require mapping the BAR (I/O or MMIO) and reading/writing the virtio device registers.
    // This function currently only reports the device and prepares for further initialization
    // by higher-level virtio driver code.

    // TODO: map BAR0 (if MMIO) using UEFI services or perform IO port accesses for IO BAR.
    // TODO: perform status ACK/DRIVER/FEATURES negotiation and setup virtqueues.

    return true;
}
