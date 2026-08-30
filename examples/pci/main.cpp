#include "drivers/pci.hpp"
#include "drivers/pci_subsystem.hpp"

void example_pci_scan()
{
    // Use the same PCI configuration helpers exposed by BlockOS.
    for (uint16_t bus = 0; bus < 256; ++bus) {
        for (uint8_t slot = 0; slot < 32; ++slot) {
            if (!pci_device_exists(static_cast<uint8_t>(bus), slot, 0))
                continue;

            const uint16_t vendor = pci_cfg_read16(
                static_cast<uint8_t>(bus), slot, 0, 0x00);
            const uint16_t device = pci_cfg_read16(
                static_cast<uint8_t>(bus), slot, 0, 0x02);

            (void)vendor;
            (void)device;
        }
    }

    pci_bus_manager.scan_all_pci_buses();
}
