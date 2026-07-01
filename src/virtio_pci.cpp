#include "virtio_pci.hpp"
#include <efi.h>
#include <efilib.h>

// Simple PCI scan for vendor ID 0x1AF4 (virtio). This is a minimal helper used for discovery only.
// This scaffold does not yet initialize devices or set up virtqueues.

static inline uint32_t pci_cfg_read32(UINT8 bus, UINT8 slot, UINT8 func, UINT8 offset) {
    // Use PCI CFG space via IO ports 0xCF8/0xCFC (legacy) — works on many QEMU setups
    uint32_t addr = (uint32_t)(0x80000000U | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) | ((uint32_t)func << 8) | (offset & 0xFC));
    __asm__ volatile ("outl %0, %%dx" : : "a" (addr), "d" (0xCF8));
    uint32_t val = 0;
    __asm__ volatile ("inl %%dx, %0" : "=a" (val) : "d" (0xCFC));
    return val;
}

void virtio_pci::scan() {
    Print(L"Scanning PCI for virtio devices...\n");
    const uint16_t virtio_vendor = 0x1AF4;
    for (uint8_t bus = 0; bus < 8; ++bus) {
        for (uint8_t slot = 0; slot < 32; ++slot) {
            for (uint8_t func = 0; func < 8; ++func) {
                uint32_t v = pci_cfg_read32(bus, slot, func, 0);
                uint16_t vendor = v & 0xFFFF;
                uint16_t device = (v >> 16) & 0xFFFF;
                if (vendor == 0xFFFF) continue; // no device
                if (vendor == virtio_vendor) {
                    CHAR16 buf[128];
                    UnicodeSPrint(buf, sizeof(buf), L"Found virtio device: bus=%d slot=%d func=%d device=0x%04x\n", bus, slot, func, device);
                    Print(buf);
                }
            }
        }
    }
}
