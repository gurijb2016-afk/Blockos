#include "pci_subsystem.hpp"
#include "pci.hpp"

#include <stdint.h>

PciSubsystem pci_bus_manager;

PciSubsystem::PciSubsystem()
    : registered_count(0)
{
    clear_registry();
}

void PciSubsystem::clear_registry()
{
    registered_count = 0;

    for (uint32_t i = 0; i < PCI_MAX_DEVICES; ++i)
    {
        device_registry[i] = {};
        device_registry[i].is_valid = false;
    }
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

void PciSubsystem::scan_function(
    uint8_t bus,
    uint8_t slot,
    uint8_t func
)
{
    if (registered_count >= PCI_MAX_DEVICES)
        return;

    if (!pci_device_exists(
            bus,
            slot,
            func))
    {
        return;
    }

    PciDevice dev{};

    dev.bus = bus;
    dev.slot = slot;
    dev.func = func;

    dev.vendor_id =
        pci_config_read_word(
            bus,
            slot,
            func,
            0x00
        );

    dev.device_id =
        pci_config_read_word(
            bus,
            slot,
            func,
            0x02
        );

    if (dev.vendor_id == 0xFFFF)
        return;

    uint32_t class_reg =
        pci_config_read_dword(
            bus,
            slot,
            func,
            0x08
        );

    dev.revision =
        static_cast<uint8_t>(
            class_reg & 0xFF
        );

    dev.prog_if =
        static_cast<uint8_t>(
            (class_reg >> 8) & 0xFF
        );

    dev.subclass =
        static_cast<uint8_t>(
            (class_reg >> 16) & 0xFF
        );

    dev.class_id =
        static_cast<uint8_t>(
            (class_reg >> 24) & 0xFF
        );

    uint32_t header =
        pci_config_read_dword(
            bus,
            slot,
            func,
            0x0C
        );

    dev.header_type =
        static_cast<uint8_t>(
            (header >> 16) & 0xFF
        );

    for (int i = 0; i < 6; ++i)
    {
        dev.bar[i] =
            pci_read_bar(
                bus,
                slot,
                func,
                i
            );
    }

    dev.is_valid = true;

    device_registry[registered_count++] = dev;
}

void PciSubsystem::scan_all_pci_buses()
{
    clear_registry();

    for (uint16_t bus = 0; bus < 256; ++bus)
    {
        for (uint8_t slot = 0; slot < 32; ++slot)
        {
            if (!pci_device_exists(
                    static_cast<uint8_t>(bus),
                    slot,
                    0))
            {
                continue;
            }

            uint8_t header_type =
                pci_cfg_read8(
                    static_cast<uint8_t>(bus),
                    slot,
                    0,
                    0x0E
                );

            uint8_t function_count =
                (header_type & 0x80)
                    ? 8
                    : 1;

            for (uint8_t func = 0;
                 func < function_count;
                 ++func)
            {
                scan_function(
                    static_cast<uint8_t>(bus),
                    slot,
                    func
                );
            }
        }
    }
}

const PciDevice* PciSubsystem::get_devices(
    uint32_t* count
) const
{
    if (count)
        *count = registered_count;

    return device_registry;
}

PciDevice* PciSubsystem::get_device_by_id(
    uint16_t vendor,
    uint16_t device
)
{
    for (uint32_t i = 0;
         i < registered_count;
         ++i)
    {
        PciDevice& d =
            device_registry[i];

        if (!d.is_valid)
            continue;

        if (d.vendor_id == vendor &&
            d.device_id == device)
        {
            return &d;
        }
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
         ++i)
    {
        PciDevice& d =
            device_registry[i];

        if (!d.is_valid)
            continue;

        if (d.bus == bus &&
            d.slot == slot &&
            d.func == func)
        {
            return &d;
        }
    }

    return nullptr;
}

uint32_t PciSubsystem::device_count() const
{
    return registered_count;
}

void PciSubsystem::configure_mmio_bars()
{
    for (uint32_t i = 0;
         i < registered_count;
         ++i)
    {
        PciDevice& d =
            device_registry[i];

        if (!d.is_valid)
            continue;

        for (int bar = 0; bar < 6; ++bar)
        {
            volatile uint64_t value =
                d.bar[bar];

            (void)value;
        }
    }
}

void PciSubsystem::print_hardware_tree_to_gui(
    uint8_t*,
    uint32_t,
    int,
    int
)
{
}
