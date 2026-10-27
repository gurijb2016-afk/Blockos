#include "hardware_autoprobe.hpp"

#include "pci_subsystem.hpp"
#include "virtio_blk.hpp"
#include "virtio_net_driver.hpp"
#include "nvme.hpp"
#include "ahci.hpp"
#include "usb_xhci.hpp"
#include "audio.hpp"
#include "net/network_hw.hpp"
#include "hardware_inventory.hpp"
#include "virtio_input.hpp"
#include "virtio_rng.hpp"
#include "virtio_pci.hpp"
#include "serial.hpp"

#include <string.h>

namespace blockos_hw {
namespace {

Report g_report{};

static void reset_report()
{
    memset(&g_report, 0, sizeof(g_report));
}

static void scan_pci()
{
    // Populate the generic PCI registry before attempting device-specific drivers.
    pci_bus_manager.scan_all_pci_buses();
    pci_bus_manager.configure_mmio_bars();
    g_report.pci_devices = pci_bus_manager.device_count();
}

static void probe_storage()
{
    g_report.virtio_block = virtio_blk::init();
    g_report.nvme = nvme::init();
    g_report.ahci = ahci::init();
}

static void probe_network()
{
    g_report.virtio_net = virtio_net::init();
    g_report.native_net = network_hw::init();
}

static void probe_usb_audio()
{
    usb::xhci::Controller xhci{};
    if (usb::xhci::probe(&xhci)) {
        if (usb::xhci::reset(&xhci) && usb::xhci::run(&xhci))
            g_report.xhci = true;
    }
    g_report.hda = audio::init();
}

static void probe_input_entropy_serial()
{
    virtio_pci::scan();
    g_report.virtio_input = virtio_input::init();
    g_report.virtio_rng = virtio_rng::init();
    g_report.serial = serial::init();
}
} // namespace

void pre_exit_bootstrap(Report* out_report)
{
    reset_report();
    scan_pci();
    probe_storage();
    probe_network();
    probe_usb_audio();
    probe_input_entropy_serial();

    // The inventory is read-only. It reports what was discovered and what
    // driver families exist, without assuming a device is initialized merely
    // because its PCI ID or class is recognized.
    hardware_inventory::print_report();
    if (out_report)
        *out_report = g_report;
}

const Report& last_report()
{
    return g_report;
}

} // namespace blockos_hw
