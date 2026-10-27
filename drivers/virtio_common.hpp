#pragma once

#include <stdint.h>
#include <stddef.h>

#include "virtqueue_ops.hpp"

namespace virtio_common {

enum class DeviceType : uint8_t {
    BLOCK = 0,
    NETWORK = 1,
    INPUT = 2,
    CONSOLE = 3,
    ENTROPY = 4,
    GPU = 16
};

enum class Transport : uint8_t {
    PCI_IO = 0,
    PCI_MMIO = 1,
    PCI_MODERN = 2
};

struct DeviceHandle {
    uint32_t device_id{};
    uint8_t bus{};
    uint8_t slot{};
    uint8_t func{};

    uint64_t bar0{};
    bool mmio{};
    bool modern{};
    Transport transport{Transport::PCI_IO};

    uint16_t vendor_id{};
    uint8_t irq{};

    // VirtIO modern PCI capability windows.
    uint64_t common_cfg{};
    uint64_t notify_cfg{};
    uint64_t device_cfg{};
    uint64_t isr_cfg{};
    uint32_t notify_off_multiplier{};

    // Feature state after negotiation.
    uint64_t host_features{};
    uint64_t negotiated_features{};

    // Selected queue size cache.
    uint16_t queue_sizes[8]{};
};

bool probe_device(DeviceType type, DeviceHandle* h);
bool device_init(DeviceHandle* h, uint64_t wanted_features = 0);

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

void set_device_status(DeviceHandle* h, uint8_t status);
uint8_t get_device_status(DeviceHandle* h);

uint16_t queue_max_size(DeviceHandle* h, uint16_t queue_index);

bool setup_queue(
    DeviceHandle* h,
    uint16_t queue_index,
    void* memory,
    uint32_t requested_size,
    VirtQueueView* out_view
);

bool notify_queue(
    DeviceHandle* h,
    uint16_t queue_index
);

uint8_t read_device_config8(
    const DeviceHandle* h,
    uint32_t offset
);

uint16_t read_device_config16(
    const DeviceHandle* h,
    uint32_t offset
);

uint32_t read_device_config32(
    const DeviceHandle* h,
    uint32_t offset
);

uint64_t read_device_config64(
    const DeviceHandle* h,
    uint32_t offset
);

uint64_t block_capacity_sectors(
    const DeviceHandle* h
);

bool block_flush_supported(
    const DeviceHandle* h
);

}
