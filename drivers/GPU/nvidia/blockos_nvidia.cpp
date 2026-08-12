#include "blockos_nvidia.hpp"
#include "drivers/pci.hpp"
#include "drivers/device_manager.hpp"

namespace blockos::nvidia {

static uint16_t read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off)
{
    return pci_cfg_read16(bus, slot, func, off);
}

static uint32_t read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off)
{
    return pci_cfg_read32(bus, slot, func, off);
}

static void fill(DeviceInfo& d, uint8_t bus, uint8_t slot, uint8_t func)
{
    d.bus = bus;
    d.slot = slot;
    d.function = func;
    d.vendor_id = read16(bus, slot, func, 0x00);
    d.device_id = read16(bus, slot, func, 0x02);
    d.command = read16(bus, slot, func, 0x04);
    d.status = read16(bus, slot, func, 0x06);
    d.prog_if = read32(bus, slot, func, 0x08) >> 8;
    d.class_id = static_cast<uint8_t>(read32(bus, slot, func, 0x08) >> 24);
    d.subclass = static_cast<uint8_t>(read32(bus, slot, func, 0x08) >> 16);
    for (int i = 0; i < 6; ++i)
        d.bar[i] = pci_read_bar(bus, slot, func, i);
    d.present = true;
}

bool probe(DeviceInfo& out)
{
    DeviceInfo tmp{};
    if (scan(&tmp, 1) == 0)
        return false;
    out = tmp;

    // Register only the actual hardware that was found.
    hardware_center.register_device(
        "NVIDIA GPU",
        DEV_TYPE_GRAPHICS,
        out.bar[0],
        0
    );
    return true;
}

uint32_t scan(DeviceInfo* out, uint32_t capacity)
{
    if (!out || capacity == 0)
        return 0;

    uint32_t found = 0;

    for (uint16_t bus = 0; bus < 256 && found < capacity; ++bus) {
        for (uint8_t slot = 0; slot < 32 && found < capacity; ++slot) {
            for (uint8_t func = 0; func < 8 && found < capacity; ++func) {
                const uint16_t vendor = read16(bus, slot, func, 0x00);
                if (vendor != NVIDIA_VENDOR_ID)
                    continue;

                const uint32_t class_reg = read32(bus, slot, func, 0x08);
                const uint8_t class_id = static_cast<uint8_t>(class_reg >> 24);
                const uint8_t subclass = static_cast<uint8_t>(class_reg >> 16);

                // PCI display controller class (0x03). Keep all NVIDIA
                // display controllers, including 3D-only class 0x03/0x02.
                if (class_id != 0x03)
                    continue;

                fill(out[found], bus, slot, func);
                ++found;
            }
        }
    }

    return found;
}

uint32_t config_read32(const DeviceInfo& dev, uint8_t offset)
{
    return pci_cfg_read32(dev.bus, dev.slot, dev.function, offset);
}

void config_write32(const DeviceInfo& dev, uint8_t offset, uint32_t value)
{
    pci_cfg_write32(dev.bus, dev.slot, dev.function, offset, value);
}

const char* generation_name(uint16_t device_id)
{
    // NVIDIA family IDs are intentionally kept as a small informational map.
    // The full open driver remains the source of truth for generation support.
    const uint16_t hi = static_cast<uint16_t>(device_id >> 4);
    switch (hi) {
        default: return "NVIDIA GPU";
    }
}

} // namespace blockos::nvidia
