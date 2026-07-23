#include "pci.hpp"
#include <efi.h>
#include <efilib.h>

static inline void outl(uint32_t value, uint16_t port) {
    __asm__ volatile ("outl %0, %1" : : "a" (value), "d" (port));
}
static inline uint32_t inl(uint16_t port) {
    uint32_t value;
    __asm__ volatile ("inl %1, %0" : "=a" (value) : "d" (port));
    return value;
}
static inline void outb(uint8_t value, uint16_t port) {
    __asm__ volatile ("outb %0, %1" : : "a" (value), "d" (port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a" (value) : "d" (port));
    return value;
}

uint32_t pci_cfg_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t addr = 0x80000000U | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) | ((uint32_t)func << 8) | (offset & 0xFC);
    outl(addr, 0xCF8);
    return inl(0xCFC);
}

uint16_t pci_cfg_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t v = pci_cfg_read32(bus, slot, func, offset & 0xFC);
    int shift = (offset & 2) * 8;
    return (uint16_t)((v >> shift) & 0xFFFF);
}

uint8_t pci_cfg_read8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t v = pci_cfg_read32(bus, slot, func, offset & 0xFC);
    int shift = (offset & 3) * 8;
    return (uint8_t)((v >> shift) & 0xFF);
}

void pci_cfg_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    uint32_t addr = 0x80000000U | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) | ((uint32_t)func << 8) | (offset & 0xFC);
    outl(addr, 0xCF8);
    outl(value, 0xCFC);
}

void pci_cfg_write16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t value) {
    uint32_t v = pci_cfg_read32(bus, slot, func, offset & 0xFC);
    int shift = (offset & 2) * 8;
    uint32_t mask = 0xFFFFu << shift;
    v = (v & ~mask) | (((uint32_t)value << shift) & mask);
    pci_cfg_write32(bus, slot, func, offset & 0xFC, v);
}

void pci_cfg_write8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint8_t value) {
    uint32_t v = pci_cfg_read32(bus, slot, func, offset & 0xFC);
    int shift = (offset & 3) * 8;
    uint32_t mask = 0xFFu << shift;
    v = (v & ~mask) | (((uint32_t)value << shift) & mask);
    pci_cfg_write32(bus, slot, func, offset & 0xFC, v);
}

uint64_t pci_read_bar(uint8_t bus, uint8_t slot, uint8_t func, int bar_index) {
    int offset = 0x10 + bar_index * 4;
    uint32_t low = pci_cfg_read32(bus, slot, func, offset);
    // Determine if 64-bit BAR
    if ((low & 0x6) == 0x4) {
        // 64-bit BAR
        uint32_t high = pci_cfg_read32(bus, slot, func, offset + 4);
        uint64_t bar = ((uint64_t)high << 32) | (low & ~0xFULL);
        return bar;
    } else {
        uint64_t bar = (uint64_t)(low & ~0xFULL);
        return bar;
    }
}

bool pci_device_exists(uint8_t bus, uint8_t slot, uint8_t func) {
    uint16_t vendor = pci_cfg_read16(bus, slot, func, 0);
    return vendor != 0xFFFF;
}
