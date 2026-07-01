#pragma once
#include <stdint.h>

namespace virtio_common {
    // Read host features (legacy 32-bit host features register)
    uint32_t read_host_features(void* bar0, bool mmio);
    // Negotiate guest features: write guest features mask (legacy)
    bool negotiate_features(void* bar0, bool mmio, uint32_t want_mask);
}
