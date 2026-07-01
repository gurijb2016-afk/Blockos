#include "virtio_common.hpp"
#include "pci.hpp"
#include "dma.hpp"
#include <efi.h>
#include <efilib.h>

static inline void outb_io(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a" (val), "dN" (port));
}
static inline uint8_t inb_io(uint16_t port) {
    uint8_t val;
    __asm__ volatile ("inb %1, %0" : "=a" (val) : "dN" (port));
    return val;
}

static inline void outl_io(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : "a" (val), "dN" (port));
}
static inline uint32_t inl_io(uint16_t port) {
    uint32_t val;
    __asm__ volatile ("inl %1, %0" : "=a" (val) : "dN" (port));
    return val;
}

static uint8_t read_reg8_mmio(uint64_t base, uint32_t offset) {
    volatile uint8_t* p = (volatile uint8_t*)(UINTN)(base + offset);
    return *p;
}
static void write_reg8_mmio(uint64_t base, uint32_t offset, uint8_t v) {
    volatile uint8_t* p = (volatile uint8_t*)(UINTN)(base + offset);
    *p = v;
}

static uint32_t read_reg32_mmio(uint64_t base, uint32_t offset) {
    volatile uint32_t* p = (volatile uint32_t*)(UINTN)(base + offset);
    return *p;
}
static void write_reg32_mmio(uint64_t base, uint32_t offset, uint32_t v) {
    volatile uint32_t* p = (volatile uint32_t*)(UINTN)(base + offset);
    *p = v;
}

// Generic register access (legacy IO or MMIO)
static uint8_t virtio_read8(const virtio_common::DeviceHandle* h, uint32_t offset) {
    if (h->mmio) return read_reg8_mmio(h->bar0, offset);
    else return inb_io((uint16_t)(h->bar0 + offset));
}
static void virtio_write8(const virtio_common::DeviceHandle* h, uint32_t offset, uint8_t v) {
    if (h->mmio) write_reg8_mmio(h->bar0, offset, v);
    else outb_io((uint16_t)(h->bar0 + offset), v);
}
static uint32_t virtio_read32(const virtio_common::DeviceHandle* h, uint32_t offset) {
    if (h->mmio) return read_reg32_mmio(h->bar0, offset);
    else return inl_io((uint16_t)(h->bar0 + offset));
}
static void virtio_write32(const virtio_common::DeviceHandle* h, uint32_t offset, uint32_t v) {
    if (h->mmio) write_reg32_mmio(h->bar0, offset, v);
    else outl_io((uint16_t)(h->bar0 + offset), v);
}

bool virtio_common::device_init(virtio_common::DeviceHandle* h) {
    if (!h) return false;
    CHAR16 buf[256];
    UnicodeSPrint(buf, sizeof(buf), L"virtio_common: initializing device id=0x%08x at %d:%d.%d BAR0=0x%lx mmio=%d\n",
                 h->device_id, h->bus, h->slot, h->func, h->bar0, h->mmio);
    Print(buf);

    // Try minimal legacy virtio status sequence if device appears to be legacy PCI virtio
    // Legacy status register is at offset 0x12 (per virtio legacy spec) for IO-space devices
    // For MMIO devices the layout differs; we attempt to write to a few common offsets conservatively.

    const uint32_t STATUS_OFFSET_LEGACY = 0x12;
    const uint8_t STATUS_ACK = 1;
    const uint8_t STATUS_DRIVER = 2;

    // Read current status
    uint8_t status = virtio_read8(h, STATUS_OFFSET_LEGACY);
    UnicodeSPrint(buf, sizeof(buf), L"virtio_common: current status=0x%02x\n", status);
    Print(buf);

    // Write ACK
    virtio_write8(h, STATUS_OFFSET_LEGACY, (uint8_t)(status | STATUS_ACK));
    // Write DRIVER
    virtio_write8(h, STATUS_OFFSET_LEGACY, (uint8_t)(status | STATUS_ACK | STATUS_DRIVER));

    uint8_t newstatus = virtio_read8(h, STATUS_OFFSET_LEGACY);
    UnicodeSPrint(buf, sizeof(buf), L"virtio_common: updated status=0x%02x\n", newstatus);
    Print(buf);

    // Allocate a small DMA buffer for virtqueue use as a test (drivers will allocate proper queues)
    void* dq = dma::alloc(4096);
    if (!dq) {
        Print(L"virtio_common: dma::alloc failed (virtqueue test)\n");
    } else {
        UnicodeSPrint(buf, sizeof(buf), L"virtio_common: dma test alloc at %p\n", dq);
        Print(buf);
    }

    // Note: full feature negotiation and virtqueue setup is implemented in specific drivers
    return true;
}
