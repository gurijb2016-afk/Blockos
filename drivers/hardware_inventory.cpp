#include "hardware_inventory.hpp"
#include "pci_id_db.hpp"
#include "pci.hpp"
#include "pci_subsystem.hpp"
#include "hardware_autoprobe.hpp"

extern "C" {
#include <efi.h>
#include <efilib.h>
}

namespace hardware_inventory {
namespace {
struct InterruptCaps { bool msi; bool msix; };

static InterruptCaps interrupt_caps(const PciDevice& d)
{
    InterruptCaps result{false, false};

    // PCI Status.Capabilities List bit (bit 4); read-only probing only.
    if ((pci_cfg_read16(d.bus, d.slot, d.func, 0x06) & 0x0010u) == 0)
        return result;

    uint8_t ptr = static_cast<uint8_t>(pci_cfg_read8(d.bus, d.slot, d.func, 0x34) & 0xFCu);
    bool seen[64]{};

    // Standard PCI capability pointers live in 0x40..0xFC. Bound traversal and
    // detect loops so malformed/virtual device config space cannot hang boot.
    for (unsigned hop = 0; hop < 48 && ptr >= 0x40 && ptr <= 0xFC; ++hop) {
        const unsigned index = ptr >> 2;
        if (index >= 64 || seen[index])
            break;
        seen[index] = true;

        const uint8_t id = pci_cfg_read8(d.bus, d.slot, d.func, ptr);
        const uint8_t next = static_cast<uint8_t>(pci_cfg_read8(d.bus, d.slot, d.func,
                                        static_cast<uint8_t>(ptr + 1)) & 0xFCu);
        if (id == 0x05) result.msi = true;
        if (id == 0x11) result.msix = true;
        if (result.msi && result.msix) break;
        if (next == 0 || next == ptr) break;
        ptr = next;
    }
    return result;
}
}

size_t count()
{
    return static_cast<size_t>(pci_bus_manager.device_count());
}

bool get(size_t index, Entry* out)
{
    if (!out || index >= count()) return false;
    PciDevice* d = pci_bus_manager.device_at(static_cast<uint32_t>(index));
    if (!d || !d->is_valid) return false;

    const InterruptCaps caps = interrupt_caps(*d);
    out->device = d;
    out->vendor = pci_id_db::vendor_name(d->vendor_id);
    out->model = pci_id_db::device_name(d->vendor_id, d->device_id);
    out->class_description = pci_id_db::class_name(d->class_id);
    out->subclass_description = pci_id_db::subclass_name(d->class_id, d->subclass, d->prog_if);
    out->driver_hint = pci_id_db::driver_hint(d->vendor_id, d->device_id,
                                                d->class_id, d->subclass, d->prog_if);
    out->has_msi = caps.msi;
    out->has_msix = caps.msix;
    return true;
}

void print_report()
{
    const blockos_hw::Report& report = blockos_hw::last_report();
    const size_t total = count();
    size_t identified = 0;
    size_t driver_hints = 0;

    for (size_t i = 0; i < total; ++i) {
        Entry e{};
        if (!get(i, &e)) continue;
        if (e.model) ++identified;
        if (e.driver_hint) ++driver_hints;
    }

    Print((CHAR16*)L"\r\nBlockOS hardware inventory: PCI=%u, device-name matches=%u, driver-family hints=%u\r\n",
          (UINTN)total, (UINTN)identified, (UINTN)driver_hints);
    Print((CHAR16*)L"Drivers detected: virtio-blk=%u nvme=%u ahci=%u virtio-net=%u native-net=%u xhci=%u hda=%u\r\n",
          (UINTN)report.virtio_block, (UINTN)report.nvme, (UINTN)report.ahci,
          (UINTN)report.virtio_net, (UINTN)report.native_net, (UINTN)report.xhci,
          (UINTN)report.hda);
    Print((CHAR16*)L"Capability check: MSI/MSI-X are reported only; they are not enabled here.\r\n");

    const size_t max_lines = 32;
    const size_t limit = total < max_lines ? total : max_lines;
    for (size_t i = 0; i < limit; ++i) {
        Entry e{};
        if (!get(i, &e) || !e.device) continue;
        const PciDevice& d = *e.device;
        const char* model = e.model ? e.model : "unlisted device ID";
        const char* driver = e.driver_hint ? e.driver_hint : "inventory only / no matched driver";
        Print((CHAR16*)L"PCI %02x:%02x.%x %04x:%04x (%a) class=%02x/%02x/%02x %a; MSI=%u MSI-X=%u; %a\r\n",
              (UINTN)d.bus, (UINTN)d.slot, (UINTN)d.func,
              (UINTN)d.vendor_id, (UINTN)d.device_id, model,
              (UINTN)d.class_id, (UINTN)d.subclass, (UINTN)d.prog_if,
              e.subclass_description, (UINTN)e.has_msi, (UINTN)e.has_msix, driver);
    }
    if (total > limit)
        Print((CHAR16*)L"... %u additional PCI functions available via hardware_inventory::get().\r\n",
              (UINTN)(total - limit));
    Print((CHAR16*)L"Unknown IDs remain visible by numeric vendor:device ID; no generic driver is forced onto them.\r\n\r\n");
}
}
