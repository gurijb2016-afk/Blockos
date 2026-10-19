#include "pci_subsystem.hpp"
#include "pci.hpp"
#include "device_manager.hpp"

PciSubsystem pci_bus_manager;

static uint64_t pci_get_bar(uint8_t bus, uint8_t slot, uint8_t func, uint8_t index)
{
    if (index >= 6) return 0;
    const uint8_t offset = static_cast<uint8_t>(0x10u + index * 4u);
    const uint32_t low = pci_cfg_read32(bus, slot, func, offset);
    if (low == 0 || low == 0xFFFFFFFFu) return 0;
    if (low & 1u) return static_cast<uint64_t>(low & ~0x3u);
    const uint32_t type = (low >> 1) & 0x3u;
    if (type == 0x2 && index < 5) {
        const uint32_t high = pci_cfg_read32(bus, slot, func,
                                  static_cast<uint8_t>(offset + 4u));
        return (static_cast<uint64_t>(high) << 32) |
               static_cast<uint64_t>(low & ~0xFu);
    }
    return static_cast<uint64_t>(low & ~0xFu);
}

PciSubsystem::PciSubsystem() : registered_count(0)
{
    for (uint32_t i = 0; i < MAX_PCI_INVENTORY; ++i) {
        device_registry[i] = {};
        device_registry[i].vendor_id = 0xFFFF;
        device_registry[i].device_id = 0xFFFF;
        device_registry[i].is_valid = false;
    }
}

uint16_t PciSubsystem::pci_config_read_word(uint8_t bus, uint8_t slot,
                                             uint8_t func, uint8_t offset)
{
    return pci_cfg_read16(bus, slot, func, offset);
}

uint32_t PciSubsystem::pci_config_read_dword(uint8_t bus, uint8_t slot,
                                               uint8_t func, uint8_t offset)
{
    return pci_cfg_read32(bus, slot, func, offset);
}

bool PciSubsystem::read_device(uint8_t bus, uint8_t slot, uint8_t func, PciDevice* out)
{
    if (!out) return false;
    const uint16_t vendor = pci_config_read_word(bus, slot, func, 0x00);
    if (vendor == 0xFFFF || vendor == 0x0000) return false;

    const uint16_t device = pci_config_read_word(bus, slot, func, 0x02);
    const uint32_t class_reg = pci_config_read_dword(bus, slot, func, 0x08);
    const uint32_t header_reg = pci_config_read_dword(bus, slot, func, 0x0C);
    const uint32_t irq_reg = pci_config_read_dword(bus, slot, func, 0x3C);

    out->bus = bus;
    out->slot = slot;
    out->func = func;
    out->vendor_id = vendor;
    out->device_id = device;
    out->revision = static_cast<uint8_t>(class_reg & 0xFFu);
    out->prog_if = static_cast<uint8_t>((class_reg >> 8) & 0xFFu);
    out->subclass = static_cast<uint8_t>((class_reg >> 16) & 0xFFu);
    out->class_id = static_cast<uint8_t>((class_reg >> 24) & 0xFFu);
    out->header_type = static_cast<uint8_t>((header_reg >> 16) & 0xFFu);
    out->irq_line = static_cast<uint8_t>(irq_reg & 0xFFu);
    out->irq_pin = static_cast<uint8_t>((irq_reg >> 8) & 0xFFu);
    for (uint8_t i = 0; i < 6;) {
        const uint8_t bar_offset = static_cast<uint8_t>(0x10u + i * 4u);
        const uint32_t raw = pci_cfg_read32(bus, slot, func, bar_offset);
        out->bar[i] = pci_get_bar(bus, slot, func, i);
        // A 64-bit memory BAR consumes the following BAR slot; do not
        // misreport its high dword as a separate resource.
        if ((raw & 1u) == 0 && ((raw >> 1) & 0x3u) == 0x2u && i < 5) {
            out->bar[i + 1] = 0;
            i = static_cast<uint8_t>(i + 2);
        } else {
            ++i;
        }
    }
    out->is_valid = true;
    return true;
}

void PciSubsystem::register_with_device_manager(const PciDevice& device)
{
    if (!device.is_valid) return;
    DeviceType type = DEV_TYPE_PCI;
    if (device.class_id == 0x01) type = DEV_TYPE_STORAGE;
    else if (device.class_id == 0x02) type = DEV_TYPE_NETWORK;
    else if (device.class_id == 0x03) type = DEV_TYPE_GRAPHICS;
    else if (device.class_id == 0x0C && device.subclass == 0x03) type = DEV_TYPE_USB;
    else if (device.class_id == 0x04) type = DEV_TYPE_AUDIO;

    char name[64];
    const char hex[] = "0123456789ABCDEF";
    name[0] = 'p'; name[1] = 'c'; name[2] = 'i'; name[3] = ':';
    name[4] = hex[(device.vendor_id >> 12) & 0xF];
    name[5] = hex[(device.vendor_id >> 8) & 0xF];
    name[6] = hex[(device.vendor_id >> 4) & 0xF];
    name[7] = hex[device.vendor_id & 0xF]; name[8] = ':';
    name[9] = hex[(device.device_id >> 12) & 0xF];
    name[10] = hex[(device.device_id >> 8) & 0xF];
    name[11] = hex[(device.device_id >> 4) & 0xF];
    name[12] = hex[device.device_id & 0xF]; name[13] = '\0';

    uint64_t mmio = 0;
    for (int i = 0; i < 6; ++i) {
        if (device.bar[i] != 0) { mmio = device.bar[i]; break; }
    }
    // This registration records discovery only. Driver initialization is separate.
    hardware_center.register_pci_device(name, type, device.bus, device.slot,
        device.func, device.vendor_id, device.device_id, device.class_id,
        device.subclass, device.prog_if, mmio, 0, device.irq_line);
}

void PciSubsystem::scan_all_pci_buses()
{
    registered_count = 0;
    for (uint32_t i = 0; i < MAX_PCI_INVENTORY; ++i)
        device_registry[i].is_valid = false;

    for (uint16_t bus = 0; bus < 256; ++bus) {
        for (uint8_t slot = 0; slot < 32; ++slot) {
            const uint8_t bus8 = static_cast<uint8_t>(bus);
            const uint16_t vendor0 = pci_config_read_word(bus8, slot, 0, 0x00);
            if (vendor0 == 0xFFFF || vendor0 == 0x0000) continue;

            const uint8_t header = pci_cfg_read8(bus8, slot, 0, 0x0E);
            const uint8_t functions = (header & 0x80u) ? 8 : 1;
            for (uint8_t func = 0; func < functions; ++func) {
                if (registered_count >= MAX_PCI_INVENTORY)
                    return;
                PciDevice dev{};
                if (!read_device(bus8, slot, func, &dev)) continue;
                device_registry[registered_count] = dev;
                register_with_device_manager(dev);
                ++registered_count;
            }
        }
    }
}

void PciSubsystem::configure_mmio_bars()
{
    for (uint32_t i = 0; i < registered_count; ++i) {
        PciDevice& dev = device_registry[i];
        if (!dev.is_valid) continue;
        uint16_t command = pci_config_read_word(dev.bus, dev.slot, dev.func, 0x04);
        bool io = false, memory = false;
        for (int b = 0; b < 6; ++b) {
            if (dev.bar[b] == 0) continue;
            const uint32_t raw = pci_cfg_read32(dev.bus, dev.slot, dev.func,
                                      static_cast<uint8_t>(0x10 + b * 4));
            if (raw & 1u) {
                io = true;
            } else {
                memory = true;
                if (((raw >> 1) & 0x3u) == 0x2u && b < 5)
                    ++b; // skip high dword of a 64-bit memory BAR
            }
        }
        if (io) command |= 0x0001;
        if (memory) command |= 0x0002;
        pci_cfg_write16(dev.bus, dev.slot, dev.func, 0x04, command);
    }
}

PciDevice* PciSubsystem::get_device_by_id(uint16_t vendor, uint16_t device)
{
    for (uint32_t i = 0; i < registered_count; ++i)
        if (device_registry[i].is_valid && device_registry[i].vendor_id == vendor &&
            device_registry[i].device_id == device) return &device_registry[i];
    return nullptr;
}

PciDevice* PciSubsystem::get_device(uint8_t bus, uint8_t slot, uint8_t func)
{
    for (uint32_t i = 0; i < registered_count; ++i) {
        PciDevice& d = device_registry[i];
        if (d.is_valid && d.bus == bus && d.slot == slot && d.func == func) return &d;
    }
    return nullptr;
}

uint32_t PciSubsystem::device_count() const { return registered_count; }

PciDevice* PciSubsystem::device_at(uint32_t index)
{
    if (index >= registered_count) return nullptr;
    return &device_registry[index];
}

void PciSubsystem::print_hardware_tree_to_gui(uint8_t* bb, uint32_t fb_w, int win_x, int win_y)
{
    (void)bb; (void)fb_w; (void)win_x; (void)win_y;
}
