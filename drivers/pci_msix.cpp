#include "pci_msix.hpp"
#include "pci_config.hpp"
extern "C" {
#include <efi.h>
}
extern "C" {
#include <efilib.h>
}

bool pci_msix_detect_and_configure(
    virtio_common::DeviceHandle* h,
    bool enable_if_found
) {
    if (!h) return false;

    uint64_t target_bar0 = (uint64_t)(UINTN)h->bar0;
    uint32_t target_lo = (uint32_t)(target_bar0 & 0xFFFFFFFFu);

    CHAR16 buf[256];

    for (uint8_t bus = 0; bus < 8; ++bus) {
        for (uint8_t dev = 0; dev < 32; ++dev) {
            for (uint8_t fn = 0; fn < 8; ++fn) {

                uint32_t v = pci_config_read32(bus, dev, fn, 0x0);
                uint16_t vendor = (uint16_t)(v & 0xFFFFu);

                if (vendor == 0xFFFFu)
                    continue;

                uint32_t bar0 = pci_config_read32(bus, dev, fn, 0x10);

                if ((bar0 & 0xFFFFFFFFu) != target_lo)
                    continue;

                UnicodeSPrint(
                    buf,
                    sizeof(buf),
                    (CHAR16*)L"pci_msix: found candidate PCI device at %u:%u.%u (vendor=0x%04x)\n",
                    bus, dev, fn, vendor
                );
                Print(buf);

                // MSI-X capability = 0x11
                uint8_t cap = pci_find_capability(
                    bus, dev, fn, 0x11
                );

                if (cap == 0) {
                    Print((CHAR16*)
                        L"pci_msix: MSI-X capability not present on this device\n"
                    );
                    return false;
                }

                UnicodeSPrint(
                    buf,
                    sizeof(buf),
                    (CHAR16*)L"pci_msix: MSI-X capability at offset 0x%02x\n",
                    cap
                );
                Print(buf);

                // Message Control
                uint32_t ctrl_dword =
                    pci_config_read32(bus, dev, fn, cap + 2);

                uint16_t msg_ctrl =
                    (uint16_t)(ctrl_dword & 0xFFFFu);

                uint16_t table_size =
                    (msg_ctrl & 0x7FFu) + 1u;

                bool function_masked =
                    (msg_ctrl & (1u << 15)) != 0;

                UnicodeSPrint(
                    buf,
                    sizeof(buf),
                    (CHAR16*)L"pci_msix: table_size=%u function_masked=%u\n",
                    table_size,
                    function_masked
                );
                Print(buf);

                // MSI-X Table
                uint32_t table_dword =
                    pci_config_read32(bus, dev, fn, cap + 4);

                uint8_t table_bir =
                    (uint8_t)(table_dword & 0x7u);

                uint32_t table_offset =
                    table_dword & ~0x7u;

                // Pending Bit Array
                uint32_t pba_dword =
                    pci_config_read32(bus, dev, fn, cap + 8);

                uint8_t pba_bir =
                    (uint8_t)(pba_dword & 0x7u);

                uint32_t pba_offset =
                    pba_dword & ~0x7u;

                UnicodeSPrint(
                    buf,
                    sizeof(buf),
                    (CHAR16*)
                    L"pci_msix: table BIR=%u offset=0x%08x "
                    L"PBA BIR=%u offset=0x%08x\n",
                    table_bir,
                    table_offset,
                    pba_bir,
                    pba_offset
                );
                Print(buf);

                if (enable_if_found && function_masked) {

                    uint32_t orig =
                        pci_config_read32(
                            bus, dev, fn, cap + 2
                        );

                    uint32_t new_dword =
                        (orig & 0xFFFF0000u) |
                        (uint32_t)(msg_ctrl & ~(1u << 15));

                    pci_config_write32(
                        bus, dev, fn, cap + 2, new_dword
                    );

                    uint32_t verify =
                        pci_config_read32(
                            bus, dev, fn, cap + 2
                        );

                    UnicodeSPrint(
                        buf,
                        sizeof(buf),
                        (CHAR16*)
                        L"pci_msix: tried to clear function mask, "
                        L"verify=0x%08x\n",
                        verify
                    );
                    Print(buf);
                }

                return true;
            }
        }
    }

    Print((CHAR16*)
        L"pci_msix: device matching BAR0 not found "
        L"in scanned PCI range\n"
    );

    return false;
}

