#include "virtio_notify.hpp"
#include <efi.h>
#include <efilib.h>
#include <string.h>

bool virtio_notify::notify(virtio_common::DeviceHandle* h, uint16_t queue_index) {
    if (!h) return false;
    // If MMIO modern device, try writing to queue_notify register if present at common offsets
    if (h->mmio) {
        // Try offsets 0x50 or 0x12 (best-effort)
        volatile uint16_t* try1 = (volatile uint16_t*)(UINTN)(h->bar0 + 0x50);
        *try1 = queue_index;
        return true;
    } else {
        uint16_t port = (uint16_t)(h->bar0 + 0x10);
        __asm__ volatile ("outw %0, %1" : : "a" (queue_index), "dN" (port));
        return true;
    }
}

bool virtio_notify::setup_msix_if_available(virtio_common::DeviceHandle* h) {
    // Best-effort stub: detecting and configuring MSI-X requires PCI-capability walking and OS IRQ handling
    // In UEFI boot environment without full PCI services we skip this. Return false to indicate MSI-X not configured.
    (void)h;
    return false;
}
