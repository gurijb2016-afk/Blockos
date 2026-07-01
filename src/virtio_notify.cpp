#include "virtio_notify.hpp"
#include "virtio_common.hpp"
#include <efi.h>
#include <efilib.h>

// Extended notify: probe several common offsets and perform write with read-back to ensure delivery.
bool virtio_notify::notify(virtio_common::DeviceHandle* h, uint16_t queue_index) {
    if (!h) return false;
    // If modern MMIO virtio, attempt to write to common notify offsets used by implementations.
    if (h->mmio) {
        uint32_t offsets[] = { 0x50, 0x10, 0x12, 0x64 };
        for (size_t i = 0; i < sizeof(offsets)/sizeof(offsets[0]); ++i) {
            volatile uint16_t* p = (volatile uint16_t*)(UINTN)(h->bar0 + offsets[i]);
            *p = queue_index;
            // read back to force write
            volatile uint16_t r = *p;
            (void)r;
        }
        return true;
    } else {
        // I/O device: try a set of ports relative to BAR0
        uint32_t ports[] = { 0x10, 0x12, 0x50 };
        for (size_t i = 0; i < sizeof(ports)/sizeof(ports[0]); ++i) {
            uint16_t port = (uint16_t)(h->bar0 + ports[i]);
            __asm__ volatile ("outw %0, %1" : : "a" (queue_index), "dN" (port));
        }
        return true;
    }
}

bool virtio_notify::setup_msix_if_available(virtio_common::DeviceHandle* h) {
    // Best-effort stub remains: in UEFI environment detailed PCI capability parsing and IRQ wiring
    // is platform-specific and often not available. Return false to indicate MSI-X not configured.
    (void)h;
    return false;
}
