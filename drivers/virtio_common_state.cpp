#include "virtio_common_state.hpp"
#include "virtio_common_features.hpp"
#include "virtio_notify.hpp"
#include <efi.h>
#include <efilib.h>

// Improve device initialization with explicit verification and rollback on failure.

bool virtio_common::perform_device_initialization(virtio_common::DeviceHandle* h, uint64_t want_features_low) {
    if (!h) return false;
    const uint8_t VIRTIO_STATUS_ACK = 1;
    const uint8_t VIRTIO_STATUS_DRIVER = 2;
    const uint8_t VIRTIO_STATUS_DRIVER_OK = 4;
    const uint8_t VIRTIO_STATUS_FEATURES_OK = 8;
    const uint8_t VIRTIO_STATUS_FAILED = 128;

    // Start from 0
    set_device_status(h, 0);

    // ACK
    set_device_status(h, VIRTIO_STATUS_ACK);
    // DRIVER
    set_device_status(h, VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER);

    // Negotiate features (try modern first, then legacy)
    bool modern_ok = false;
    extern bool virtio_common::negotiate_modern_features(virtio_common::DeviceHandle* h, uint64_t want_mask_low);
    if (h->mmio) {
        modern_ok = virtio_common::negotiate_modern_features(h, want_features_low);
    }
    if (!modern_ok) {
        // Fall back to legacy 32-bit negotiation
        virtio_common::negotiate_features((void*)(UINTN)h->bar0, h->mmio, (uint32_t)(want_features_low & 0xFFFFFFFFu));
    }

    // Ask device to check features OK
    set_device_status(h, VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK);

    // Read back status
    uint8_t st = get_device_status(h);
    if (st & VIRTIO_STATUS_FAILED) {
        // Device rejected features. Roll back and report failure.
        CHAR16 buf[128];
        UnicodeSPrint(buf, sizeof(buf), L"virtio_common: device rejected FEATURES_OK (status=0x%02x), rolling back\n", st);
        Print(buf);
        // Reset device status to 0 to leave device in a known state
        set_device_status(h, 0);
        return false;
    }

    // Final driver OK
    set_device_status(h, VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK | VIRTIO_STATUS_DRIVER_OK);
    st = get_device_status(h);
    if (st & VIRTIO_STATUS_FAILED) {
        CHAR16 buf[128];
        UnicodeSPrint(buf, sizeof(buf), L"virtio_common: device FAILED after DRIVER_OK (status=0x%02x)\n", st);
        Print(buf);
        // Attempt rollback
        set_device_status(h, 0);
        return false;
    }

    // Attempt to enable MSI-X if available to improve interrupt handling.
    bool msix_ok = virtio_notify::setup_msix_if_available(h);
    if (msix_ok) {
        Print(L"virtio_common: MSI-X setup succeeded or found\n");
    } else {
        Print(L"virtio_common: MSI-X not available or not configured, using legacy/notify fallback\n");
    }

    CHAR16 buf[128];
    UnicodeSPrint(buf, sizeof(buf), L"virtio_common: device initialized successfully (status=0x%02x)\n", st);
    Print(buf);
    return true;
}
