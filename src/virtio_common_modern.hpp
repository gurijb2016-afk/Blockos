#pragma once
#include <stdint.h>
#include "virtio_common.hpp"

namespace virtio_common {
    // Try to detect modern MMIO virtio device and perform feature negotiation.
    // Returns true if negotiation succeeded (or legacy negotiation was applied).
    bool negotiate_modern_features(virtio_common::DeviceHandle* h, uint64_t want_mask_low);

    // Program modern-style queue addresses (desc,avail,used) for queue 'index'.
    // Returns true on success (best-effort).
    bool program_modern_queue_addr(virtio_common::DeviceHandle* h, uint16_t queue_index, uint64_t desc, uint64_t avail, uint64_t used);
}
