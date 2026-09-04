#include "pci_subsystem.hpp"
#include "pci.hpp"
#include "device_manager.hpp"

PciSubsystem pci_bus_manager;

static uint64_t pci_get_bar(
    uint8_t bus,
    uint8_t slot,
    uint8_t func,
    uint8_t index
)
{
    if (index >= 6)
        return 0;

    uint32_t low = pci_cfg_read32(
        bus,
        slot,
        func,
        0x10 + index * 4
    );

    if (low == 0)
        return 0;

    if ((low & 0x1) != 0)
        return (uint64_t)(low & ~0x3u);

    uint32_t type = (low >> 1) & 0x3;

    if (type == 0x2 && index < 5) {
        uint32_t high = pci_cfg_read32(
            bus,
            slot,
            func,
            0x10 + (index + 1) * 4
        );

        return ((uint64_t)high << 32) |
               (uint64_t)(low & ~0xFULL);
    }

    return (uint64_t)(low & ~0xFULL);
}

PciSubsystem::PciSubsystem()
    : registered_count(0)
{
    for (uint32_t i = 0; i < 64; ++i) {
        device_registry[i].bus = 0;
        device_registry[i].slot = 0;
        device_registry[i].func = 0;

        device_registry[i].vendor_id = 0xFFFF;
        device_registry[i].device_id = 0xFFFF;

        device_registry[i].revision = 0;
        device_registry[i].prog_if = 0;
        device_registry[i].subclass = 0;
        device_registry[i].class_id = 0;

        device_registry[i].header_type = 0;
        device_registry[i].irq_line = 0;
        device_registry[i].irq_pin = 0;

        for (int b = 0; b < 6; ++b)
            device_registry[i].bar[b] = 0;

        device_registry[i].is_valid = false;
    }
}

uint16_t PciSubsystem::pci_config_read_word(
    uint8_t bus,
    uint8_t slot,
    uint8_t func,
    uint8_t offset
)
{
    return pci_cfg_read16(
        bus,
        slot,
        func,
        offset
    );
}

uint32_t PciSubsystem::pci_config_read_dword(
    uint8_t bus,
    uint8_t slot,
    uint8_t func,
    uint8_t offset
)
{
    return pci_cfg_read32(
        bus,
        slot,
        func,
        offset
    );
}

bool PciSubsystem::read_device(
    uint8_t bus,
    uint8_t slot,
    uint8_t func,
    PciDevice* out
)
{
    if (!out)
        return false;

    uint16_t vendor = pci_config_read_word(
        bus,
        slot,
        func,
        0x00
    );

    if (vendor == 0xFFFF)
        return false;

    uint16_t device = pci_config_read_word(
        bus,
        slot,
        func,
        0x02
    );

    uint32_t class_reg = pci_config_read_dword(
        bus,
        slot,
        func,
        0x08
    );

    uint32_t header_reg = pci_config_read_dword(
        bus,
        slot,
        func,
        0x0C
    );

    uint32_t irq_reg = pci_config_read_dword(
        bus,
        slot,
        func,
        0x3C
    );

    out->bus = bus;
    out->slot = slot;
    out->func = func;

    out->vendor_id = vendor;
    out->device_id = device;

    out->revision = (uint8_t)(class_reg & 0xFF);
    out->prog_if = (uint8_t)((class_reg >> 8) & 0xFF);
    out->subclass = (uint8_t)((class_reg >> 16) & 0xFF);
    out->class_id = (uint8_t)((class_reg >> 24) & 0xFF);

    out->header_type = (uint8_t)((header_reg >> 16) & 0xFF);

    out->irq_line = (uint8_t)(irq_reg & 0xFF);
    out->irq_pin = (uint8_t)((irq_reg >> 8) & 0xFF);

    for (uint8_t i = 0; i < 6; ++i)
        out->bar[i] = pci_get_bar(bus, slot, func, i);

    out->is_valid = true;

    return true;
}

void PciSubsystem::register_with_device_manager(
    const PciDevice& device
)
{
    if (!device.is_valid)
        return;

    DeviceType type = DEV_TYPE_PCI;

    if (device.class_id == 0x01)
        type = DEV_TYPE_STORAGE;
    else if (device.class_id == 0x02)
        type = DEV_TYPE_NETWORK;
    else if (device.class_id == 0x03)
        type = DEV_TYPE_GRAPHICS;
    else if (device.class_id == 0x0C &&
             device.subclass == 0x03)
        type = DEV_TYPE_USB;
    else if (device.class_id == 0x04)
        type = DEV_TYPE_AUDIO;

    char name[64];

    name[0] = 'p';
    name[1] = 'c';
    name[2] = 'i';
    name[3] = ':';

    const char hex[] = "0123456789ABCDEF";

    name[4] = hex[(device.vendor_id >> 12) & 0xF];
    name[5] = hex[(device.vendor_id >> 8) & 0xF];
    name[6] = hex[(device.vendor_id >> 4) & 0xF];
    name[7] = hex[device.vendor_id & 0xF];

    name[8] = ':';

    name[9] = hex[(device.device_id >> 12) & 0xF];
    name[10] = hex[(device.device_id >> 8) & 0xF];
    name[11] = hex[(device.device_id >> 4) & 0xF];
    name[12] = hex[device.device_id & 0xF];

    name[13] = '\0';

    uint64_t mmio = 0;

    for (int i = 0; i < 6; ++i) {
        if (device.bar[i] != 0) {
            mmio = device.bar[i];
            break;
        }
    }

    hardware_center.register_pci_device(
        name,
        type,
        device.bus,
        device.slot,
        device.func,
        device.vendor_id,
        device.device_id,
        device.class_id,
        device.subclass,
        device.prog_if,
        mmio,
        0,
        device.irq_line
    );
}

void PciSubsystem::scan_all_pci_buses()
{
    registered_count = 0;

    for (uint32_t i = 0; i < 64; ++i)
        device_registry[i].is_valid = false;

    for (uint16_t bus = 0; bus < 256; ++bus) {
        for (uint8_t slot = 0; slot < 32; ++slot) {

            uint16_t vendor0 = pci_config_read_word(
                (uint8_t)bus,
                slot,
                0,
                0x00
            );

            if (vendor0 == 0xFFFF)
                continue;

            uint8_t header = pci_cfg_read8(
                (uint8_t)bus,
                slot,
                0,
                0x0E
            );

            uint8_t functions =
                (header & 0x80) ? 8 : 1;

            for (uint8_t func = 0;
                 func < functions;
                 ++func) {

                if (registered_count >= 64)
                    return;

                PciDevice dev;

                if (!read_device(
                        (uint8_t)bus,
                        slot,
                        func,
                        &dev))
                    continue;

                device_registry[registered_count] = dev;

                register_with_device_manager(dev);

                ++registered_count;
            }
        }
    }
}

void PciSubsystem::configure_mmio_bars()
{
    for (uint32_t i = 0;
         i < registered_count;
         ++i) {

        PciDevice& dev = device_registry[i];

        if (!dev.is_valid)
            continue;

        uint16_t command = pci_config_read_word(
            dev.bus,
            dev.slot,
            dev.func,
            0x04
        );

        bool io = false;
        bool memory = false;

        for (int b = 0; b < 6; ++b) {
            if (dev.bar[b] == 0)
                continue;

            uint32_t raw = pci_cfg_read32(
                dev.bus,
                dev.slot,
                dev.func,
                0x10 + b * 4
            );

            if (raw & 1)
                io = true;
            else
                memory = true;
        }

        if (io)
            command |= 0x0001;

        if (memory)
            command |= 0x0002;

        pci_cfg_write16(
            dev.bus,
            dev.slot,
            dev.func,
            0x04,
            command
        );
    }
}

PciDevice* PciSubsystem::get_device_by_id(
    uint16_t vendor,
    uint16_t device
)
{
    for (uint32_t i = 0;
         i < registered_count;
         ++i) {

        if (!device_registry[i].is_valid)
            continue;

        if (device_registry[i].vendor_id == vendor &&
            device_registry[i].device_id == device)
            return &device_registry[i];
    }

    return nullptr;
}

PciDevice* PciSubsystem::get_device(
    uint8_t bus,
    uint8_t slot,
    uint8_t func
)
{
    for (uint32_t i = 0;
         i < registered_count;
         ++i) {

        PciDevice& dev = device_registry[i];

        if (!dev.is_valid)
            continue;

        if (dev.bus == bus &&
            dev.slot == slot &&
            dev.func == func)
            return &dev;
    }

    return nullptr;
}

uint32_t PciSubsystem::device_count() const
{
    return registered_count;
}

PciDevice* PciSubsystem::device_at(
    uint32_t index
)
{
    if (index >= registered_count)
        return nullptr;

    return &device_registry[index];
}

void PciSubsystem::print_hardware_tree_to_gui(
    uint8_t* bb,
    uint32_t fb_w,
    int win_x,
    int win_y
)
{
    (void)bb;
    (void)fb_w;
    (void)win_x;
    (void)win_y;
    }
