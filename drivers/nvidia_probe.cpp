#include "nvidia_probe.hpp"

#include "pci.hpp"
#include "pcie.hpp"

namespace nvidia_probe {
namespace {
constexpr uint16_t NVIDIA_VENDOR_ID = 0x10DE;
constexpr uint8_t PCI_CLASS_DISPLAY = 0x03;
constexpr size_t PCI_INVENTORY_CAPACITY = 1024;

// Static storage avoids adding a 10+ KiB array to the boot/kernel stack.
// This probe is intended to be called serially during driver enumeration.
pcie::Device g_pci_devices[PCI_INVENTORY_CAPACITY];

static bool fill_info(const pcie::Device& dev, DeviceInfo* out)
{
    const uint32_t bar0_low = pci_cfg_read32(dev.bus, dev.slot, dev.func, 0x10);
    const bool is_io = (bar0_low & 1u) != 0;
    const uint8_t bar_type = static_cast<uint8_t>((bar0_low >> 1) & 0x3u);
    const bool is_64 = !is_io && bar_type == 0x2u;
    const uint64_t base = pci_read_bar(dev.bus, dev.slot, dev.func, 0);

    out->bus = dev.bus;
    out->slot = dev.slot;
    out->function = dev.func;
    out->vendor_id = dev.vendor;
    out->device_id = dev.device;
    out->revision_id = pci_cfg_read8(dev.bus, dev.slot, dev.func, 0x08);
    out->class_code = dev.class_code;
    out->subclass = dev.subclass;
    out->pci_command = pci_cfg_read16(dev.bus, dev.slot, dev.func, 0x04);
    out->bar0_base = base;
    out->bar0_is_io = is_io;
    out->bar0_is_64_bit = is_64;
    out->bar0_prefetchable = !is_io && ((bar0_low & 0x8u) != 0);

    return !is_io && base != 0;
}
} // namespace

Status probe_first(DeviceInfo* out)
{
    if (out == nullptr)
        return Status::InvalidArgument;

    const size_t total = pcie::enumerate(g_pci_devices, PCI_INVENTORY_CAPACITY);
    const size_t stored = total < PCI_INVENTORY_CAPACITY ? total : PCI_INVENTORY_CAPACITY;

    for (size_t i = 0; i < stored; ++i) {
        const pcie::Device& dev = g_pci_devices[i];
        if (dev.vendor != NVIDIA_VENDOR_ID || dev.class_code != PCI_CLASS_DISPLAY)
            continue;

        if (!fill_info(dev, out))
            return Status::Bar0Unavailable;
        return Status::Found;
    }

    if (total > PCI_INVENTORY_CAPACITY)
        return Status::PciInventoryTruncated;
    return Status::NotFound;
}

const char* status_string(Status status)
{
    switch (status) {
        case Status::Found: return "NVIDIA display device found (PCI probe only)";
        case Status::NotFound: return "No NVIDIA display-class PCI device found";
        case Status::InvalidArgument: return "Invalid output pointer";
        case Status::PciInventoryTruncated: return "PCI inventory exceeds probe capacity";
        case Status::Bar0Unavailable: return "NVIDIA device found, but BAR0 is not usable memory MMIO";
        default: return "Unknown NVIDIA probe status";
    }
}

} // namespace nvidia_probe
