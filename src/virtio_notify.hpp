#pragma once
#include <stdint.h>
#include "virtio_common.hpp"

namespace virtio_notify {
    // Notify device queue 'index' using the best-known mechanism (legacy or modern). Returns true on success.
    bool notify(virtio_common::DeviceHandle* h, uint16_t queue_index);

    // Setup MSI-X if available for the PCI device (best-effort). Returns true if MSI-X appears configured.
    bool setup_msix_if_available(virtio_common::DeviceHandle* h);
}
