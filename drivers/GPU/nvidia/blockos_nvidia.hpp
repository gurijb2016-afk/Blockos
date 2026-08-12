#pragma once

#include <stdint.h>

namespace blockos::nvidia {

constexpr uint16_t NVIDIA_VENDOR_ID = 0x10DE;

struct DeviceInfo {
    uint8_t bus;
    uint8_t slot;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_id;
    uint8_t subclass;
    uint8_t prog_if;
    uint64_t bar[6];
    uint16_t command;
    uint16_t status;
    bool present;
};

// Enumerate PCI and find the first NVIDIA display controller.
bool probe(DeviceInfo& out);

// Scan all PCI functions and return the number of NVIDIA GPUs found.
uint32_t scan(DeviceInfo* out, uint32_t capacity);

// Read/write NVIDIA PCI config space through the existing BlockOS PCI layer.
uint32_t config_read32(const DeviceInfo& dev, uint8_t offset);
void config_write32(const DeviceInfo& dev, uint8_t offset, uint32_t value);

const char* generation_name(uint16_t device_id);

} // namespace blockos::nvidia
