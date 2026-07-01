#include "virtio_notify.hpp"
#include "virtio_common.hpp"
#include "pci_msix.hpp"
#include <efi.h>
#include <efilib.h>
#include <string.h>

bool virtio_notify::notify(virtio_common::DeviceHandle* h, uint16_t queue_index) {
    if (!h) return false;
    if (h->mmio) {
        uint32_t offsets[] = { 0x50, 0x10, 0x12, 0x64 };
        for (size_t i = 0; i < sizeof(offsets)/sizeof(offsets[0]); ++i) {
            volatile uint16_t* p = (volatile uint16_t*)(UINTN)(h->bar0 + offsets[i]);
            *p = queue_index;
            volatile uint16_t r = *p;
            (void)r;
        }
        return true;
    } else {
        uint32_t ports[] = { 0x10, 0x12, 0x50 };
        for (size_t i = 0; i < sizeof(ports)/sizeof(ports[0]); ++i) {
            uint16_t port = (uint16_t)(h->bar0 + ports[i]);
            __asm__ volatile ("outw %0, %1" : : "a" (queue_index), "dN" (port));
        }
        return true;
    }
}

bool virtio_notify::setup_msix_if_available(virtio_common::DeviceHandle* h) {
    if (!h) return false;
    // Best-effort: try to detect MSI-X by scanning PCI space to locate the device with matching BAR0, then
    // inspect the MSI-X capability and (optionally) enable function mask clearing. We do not attempt to
    // allocate OS IRQ vectors here; this is a platform-specific step.
    bool found = pci_msix_detect_and_configure(h, false);
    if (found) {
        Print(L"virtio_notify: MSI-X capability detected (not enabled)\n");
        return true;
    }
    return false;
}
