#pragma once
#include <cstdint>

namespace virtio_pci {
    struct DeviceInfo { uint16_t vendor; uint16_t device; uint8_t bus; uint8_t slot; uint8_t func; };
    void scan(); // scan PCI bus for virtio devices and print them via UEFI Print
}
