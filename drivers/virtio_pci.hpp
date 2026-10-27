#pragma once
#include <stdint.h>

namespace virtio_pci {

struct DeviceInfo {
    uint16_t vendor;
    uint16_t device;
    uint8_t bus;
    uint8_t slot;
    uint8_t func;
    uint64_t bar0;
    bool is_modern;
    const char* type_name;
};

void scan();
uint32_t count();
const DeviceInfo* get(uint32_t index);

} // namespace virtio_pci
