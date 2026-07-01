#pragma once
#include "virtio_common.hpp"

namespace virtio_common {
    // Set the device status register (legacy PCI/IO or MMIO). value is an 8-bit status field.
    void set_device_status(virtio_common::DeviceHandle* h, uint8_t status);
    uint8_t get_device_status(virtio_common::DeviceHandle* h);

    // Perform the canonical virtio device initialization state transitions:
    // ACK -> DRIVER -> negotiate features -> FEATURES_OK -> DRIVER_OK
    // Returns true on success (device reports FEATURES_OK and not in error state).
    bool perform_device_initialization(virtio_common::DeviceHandle* h, uint64_t want_features_low);
}
