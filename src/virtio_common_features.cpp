#include "virtio_common_features.hpp"
#include <efi.h>
#include <efilib.h>

static inline uint32_t read_reg32_mmio(uint64_t base, uint32_t offset) {
    volatile uint32_t* p = (volatile uint32_t*)(UINTN)(base + offset);
    return *p;
}
static inline void write_reg32_mmio(uint64_t base, uint32_t offset, uint32_t v) {
    volatile uint32_t* p = (volatile uint32_t*)(UINTN)(base + offset);
    *p = v;
}

static inline uint32_t inl_io(uint16_t port) {
    uint32_t val;
    __asm__ volatile ("inl %1, %0" : "=a" (val) : "dN" (port));
    return val;
}
static inline void outl_io(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : "a" (val), "dN" (port));
}

uint32_t virtio_common::read_host_features(void* bar0, bool mmio) {
    const uint32_t HOST_FEATURES = 0x00;
    if (mmio) return read_reg32_mmio((uint64_t)(UINTN)bar0, HOST_FEATURES);
    else return inl_io((uint16_t)((uint64_t)(UINTN)bar0 + HOST_FEATURES));
}

bool virtio_common::negotiate_features(void* bar0, bool mmio, uint32_t want_mask) {
    const uint32_t GUEST_FEATURES = 0x04;
    uint32_t host = virtio_common::read_host_features(bar0, mmio);
    uint32_t agreed = host & want_mask;
    if (mmio) write_reg32_mmio((uint64_t)(UINTN)bar0, GUEST_FEATURES, agreed);
    else outl_io((uint16_t)((uint64_t)(UINTN)bar0 + GUEST_FEATURES), agreed);
    CHAR16 buf[128];
    UnicodeSPrint(buf, sizeof(buf), L"virtio_common: host_features=0x%08x want=0x%08x agreed=0x%08x\n", host, want_mask, agreed);
    Print(buf);
    return true;
}
