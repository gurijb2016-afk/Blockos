#include "pci_config.hpp"
#include <efi.h>
#include <efilib.h>

static inline void outl_io(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : "a" (val), "dN" (port));
}
static inline uint32_t inl_io(uint16_t port) {
    uint32_t val;
    __asm__ volatile ("inl %1, %0" : "=a" (val) : "dN" (port));
    return val;
}

uint32_t pci_config_read32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t address = 0x80000000u
        | ((uint32_t)bus << 16)
        | ((uint32_t)device << 11)
        | ((uint32_t)function << 8)
        | ((uint32_t)offset & 0xFC);
    outl_io(0xCF8, address);
    return inl_io(0xCFC);
}

void pci_config_write32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value) {
    uint32_t address = 0x80000000u
        | ((uint32_t)bus << 16)
        | ((uint32_t)device << 11)
        | ((uint32_t)function << 8)
        | ((uint32_t)offset & 0xFC);
    outl_io(0xCF8, address);
    outl_io(0xCFC, value);
}

uint8_t pci_find_capability(uint8_t bus, uint8_t device, uint8_t function, uint8_t cap_id) {
    // Read header type to ensure this device has capabilities list
    uint32_t header0 = pci_config_read32(bus, device, function, 0x0);
    uint8_t header_type = (uint8_t)((pci_config_read32(bus, device, function, 0xC) >> 16) & 0xFF);
    // Status register bit 4 indicates capabilities list
    uint16_t status = (uint16_t)((pci_config_read32(bus, device, function, 0x4) >> 16) & 0xFFFF);
    if ((status & (1 << 4)) == 0) return 0;
    // capabilities pointer at offset 0x34 for type 0 headers
    uint8_t ptr = (uint8_t)(pci_config_read32(bus, device, function, 0x34) & 0xFF);
    int safecount = 0;
    while (ptr != 0 && safecount++ < 64) {
        uint32_t cap = pci_config_read32(bus, device, function, ptr & 0xFC);
        uint8_t cid = (uint8_t)(cap & 0xFF);
        uint8_t next = (uint8_t)((cap >> 8) & 0xFF);
        if (cid == cap_id) return ptr;
        ptr = next;
    }
    return 0;
}
