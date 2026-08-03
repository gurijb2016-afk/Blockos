#pragma once

#include <stdint.h>

namespace virtio_common {

enum class DeviceType : uint8_t {
    BLOCK = 0,
    NETWORK = 1,
    INPUT = 2
};

struct DeviceHandle {
    uint32_t device_id;
    uint8_t bus;
    uint8_t slot;
    uint8_t func;

    uint64_t bar0;
    bool mmio;

    uint16_t vendor_id;
    uint8_t irq;
};

bool probe_device(DeviceType type, DeviceHandle* h);

bool device_init(DeviceHandle* h);

uint32_t read_host_features(void* bar0, bool mmio);

bool negotiate_features(void* bar0, bool mmio, uint32_t want_mask);

bool negotiate_modern_features(
    DeviceHandle* h,
    uint64_t want_mask_low
);

bool program_modern_queue_addr(
    DeviceHandle* h,
    uint16_t queue_index,
    uint64_t desc,
    uint64_t avail,
    uint64_t used
);

}
