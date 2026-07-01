#include "virtio_common_state.hpp"
#include "virtio_common_features.hpp"
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

void virtio_common::set_device_status(virtio_common::DeviceHandle* h, uint8_t status) {
    if (!h) return;
    const uint32_t DEVICE_STATUS = 0x12; // legacy PCI/IO offset for status
    if (h->mmio) {
        volatile uint8_t* p = (volatile uint8_t*)(UINTN)(h->bar0 + DEVICE_STATUS);
        *p = status;
    } else {
        uint16_t port = (uint16_t)(h->bar0 + DEVICE_STATUS);
        outb_io(port, status);
    }
}

uint8_t virtio_common::get_device_status(virtio_common::DeviceHandle* h) {
    if (!h) return 0;
    const uint32_t DEVICE_STATUS = 0x12;
    if (h->mmio) {
        volatile uint8_t* p = (volatile uint8_t*)(UINTN)(h->bar0 + DEVICE_STATUS);
        return *p;
    } else {
        uint16_t port = (uint16_t)(h->bar0 + DEVICE_STATUS);
        return inb_io(port);
    }
}

bool virtio_common::perform_device_initialization(virtio_common::DeviceHandle* h, uint64_t want_features_low) {
    if (!h) return false;
    // Step 1: set ACK
    const uint8_t VIRTIO_STATUS_ACK = 1;
    const uint8_t VIRTIO_STATUS_DRIVER = 2;
    const uint8_t VIRTIO_STATUS_FEATURES_OK = 8;
    const uint8_t VIRTIO_STATUS_DRIVER_OK = 4;
    const uint8_t VIRTIO_STATUS_FAILED = 128;

    // ack
    set_device_status(h, VIRTIO_STATUS_ACK);
    // driver
    set_device_status(h, VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER);

    // Negotiate features (best-effort) using modern and legacy helpers
    // Try modern first
    bool modern_ok = false;
    extern bool virtio_common::negotiate_modern_features(virtio_common::DeviceHandle* h, uint64_t want_mask_low);
    if (h->mmio) {
        modern_ok = virtio_common::negotiate_modern_features(h, want_features_low);
    }
    if (!modern_ok) {
        // Fall back to legacy 32-bit negotiation
        virtio_common::negotiate_features((void*)(UINTN)h->bar0, h->mmio, (uint32_t)(want_features_low & 0xFFFFFFFFu));
    }

    // set FEATURES_OK
    set_device_status(h, VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK);

    // read back status to ensure device didn't set FAILED
    uint8_t st = get_device_status(h);
    if (st & VIRTIO_STATUS_FAILED) return false;

    // Finalize driver OK
    set_device_status(h, VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK | VIRTIO_STATUS_DRIVER_OK);
    st = get_device_status(h);
    if (st & VIRTIO_STATUS_FAILED) return false;

    return true;
}
