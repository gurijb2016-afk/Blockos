#pragma once

#include <stdint.h>

namespace nvidia_probe {

enum class Status : uint8_t {
    Found = 0,
    NotFound,
    InvalidArgument,
    PciInventoryTruncated,
    Bar0Unavailable
};

struct DeviceInfo {
    uint8_t bus;
    uint8_t slot;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t revision_id;
    uint8_t class_code;
    uint8_t subclass;
    uint32_t pci_command;
    uint64_t bar0_base;
    bool bar0_is_io;
    bool bar0_is_64_bit;
    bool bar0_prefetchable;
};

// Read-only PCI identification of the first NVIDIA display-class device.
// Does not enable PCI memory decoding or bus mastering, touch MMIO, install
// interrupts, or initialize the GPU. Caller owns *out.
Status probe_first(DeviceInfo* out);
const char* status_string(Status status);

} // namespace nvidia_probe
