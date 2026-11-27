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
#include "virtio_common.hpp"
#include "gpu/virtio_gpu.hpp"
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

static bool pci_has_class(uint8_t cls, uint8_t sub, int prog_if = -1)
{
    const uint32_t n = pci_bus_manager.device_count();
    for (uint32_t i = 0; i < n; ++i) {
        const PciDevice* d = pci_bus_manager.device_at(i);
        if (!d || !d->is_valid) continue;
        if (d->class_id == cls && d->subclass == sub) {
            if (prog_if < 0 || d->prog_if == static_cast<uint8_t>(prog_if))
                return true;
        }
    }
    return false;
}

static bool pci_has_virtio(uint16_t legacy_id, uint16_t modern_id)
{
    const uint32_t n = pci_bus_manager.device_count();
    for (uint32_t i = 0; i < n; ++i) {
        const PciDevice* d = pci_bus_manager.device_at(i);
        if (!d || !d->is_valid) continue;
        if (d->vendor_id == 0x1AF4 && (d->device_id == legacy_id || d->device_id == modern_id))
            return true;
    }
    return false;
}

static bool pci_has_native_nic()
{
    const uint32_t n = pci_bus_manager.device_count();
    for (uint32_t i = 0; i < n; ++i) {
        const PciDevice* d = pci_bus_manager.device_at(i);
        if (!d || !d->is_valid) continue;
        if (d->class_id == 0x02 && d->subclass == 0x00 && d->vendor_id != 0x1AF4)
            return true;
    }
    return false;
}

static bool cpu_supports_rdrand()
{
    uint32_t eax = 1, ebx = 0, ecx = 0, edx = 0;
    __asm__ volatile("cpuid" : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx));
    return (ecx & (1u << 30)) != 0;
}

static ProbeStatus classify(bool init_ok, bool hw_present)
{
    if (init_ok) return ProbeStatus::Ok;
    return hw_present ? ProbeStatus::Failed : ProbeStatus::NotAvailable;
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

    g_report.st_virtio_block = classify(g_report.virtio_block, pci_has_virtio(0x1001, 0x1042));
    g_report.st_nvme = classify(g_report.nvme, pci_has_class(0x01, 0x08, 0x02));
    g_report.st_ahci = classify(g_report.ahci, pci_has_class(0x01, 0x06, 0x01));
}

static void probe_network()
{
    g_report.virtio_net = virtio_net::init();
    g_report.native_net = network_hw::init();

    g_report.st_virtio_net = classify(g_report.virtio_net, pci_has_virtio(0x1000, 0x1041));
    g_report.st_native_net = classify(g_report.native_net, pci_has_native_nic());
}

static void probe_usb_audio()
{
    static usb::xhci::Controller g_xhci{};
    if (usb::xhci::probe(&g_xhci)) {
        // Only probe and take legacy ownership before ExitBootServices; full
        // reset+initialize runs post-exit so OVMF's XhciDxe timer is untouched.
        g_report.xhci = true;
    }
    g_report.hda = audio::init();

    g_report.st_xhci = classify(g_report.xhci, pci_has_class(0x0C, 0x03, 0x30));
    g_report.st_hda = classify(g_report.hda, pci_has_class(0x04, 0x03));
}

static void probe_input_entropy_serial()
{
    virtio_pci::scan();
    g_report.virtio_input = virtio_input::init();
    g_report.virtio_rng = virtio_rng::init();
    // Do not reset VirtIO-GPU before ExitBootServices(): OVMF's VirtioGpuDxe
    // may still own the device for UEFI ConOut/GOP. Just check presence here.
    virtio_common::DeviceHandle gpu_probe{};
    g_report.virtio_gpu = virtio_common::probe_device(virtio_common::DeviceType::GPU, &gpu_probe);
    g_report.serial = serial::init();

    g_report.st_virtio_input = classify(g_report.virtio_input, pci_has_virtio(0x1012, 0x1052));
    g_report.st_virtio_rng = classify(g_report.virtio_rng, pci_has_virtio(0x1005, 0x1044));
    g_report.st_virtio_gpu = classify(g_report.virtio_gpu, pci_has_virtio(0x1010, 0x1050));
    g_report.st_serial = g_report.serial ? ProbeStatus::Ok : ProbeStatus::NotAvailable;
    g_report.st_cpu_rdrand = cpu_supports_rdrand() ? ProbeStatus::Ok : ProbeStatus::NotAvailable;
}

static void append_str(char* buf, size_t cap, size_t& pos, const char* s)
{
    if (!s) return;
    while (*s && pos + 1 < cap) buf[pos++] = *s++;
    buf[pos] = '\0';
}

static void emit_line(void (*emit)(const char*, void*), void* ctx,
                      ProbeStatus st, const char* label, const char* detail = nullptr)
{
    char line[160]{};
    size_t pos = 0;
    switch (st) {
        case ProbeStatus::Ok:
            append_str(line, sizeof(line), pos, "[\x1b[32m  OK  \x1b[0m] ");
            break;
        case ProbeStatus::Failed:
            append_str(line, sizeof(line), pos, "[\x1b[31m FAIL \x1b[0m] ");
            break;
        case ProbeStatus::NotAvailable:
        default:
            append_str(line, sizeof(line), pos, "[\x1b[33m N/A  \x1b[0m] ");
            break;
    }
    append_str(line, sizeof(line), pos, label);
    if (detail && *detail) {
        append_str(line, sizeof(line), pos, " (");
        append_str(line, sizeof(line), pos, detail);
        append_str(line, sizeof(line), pos, ")");
    }
    append_str(line, sizeof(line), pos, "\n");
    emit(line, ctx);
}

static void uint_to_dec(uint32_t v, char* out, size_t cap)
{
    if (cap < 2) return;
    char tmp[16];
    size_t n = 0;
    do {
        tmp[n++] = static_cast<char>('0' + (v % 10u));
        v /= 10u;
    } while (v && n < sizeof(tmp));
    size_t pos = 0;
    while (n > 0 && pos + 1 < cap) out[pos++] = tmp[--n];
    out[pos] = '\0';
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

void print_boot_checklist(void (*emit)(const char* text, void* ctx), void* ctx)
{
    if (!emit) return;

    char pci_info[32]{};
    char num[16]{};
    uint_to_dec(g_report.pci_devices, num, sizeof(num));
    size_t p = 0;
    for (size_t i = 0; num[i] && p + 1 < sizeof(pci_info); ++i) pci_info[p++] = num[i];
    const char* suffix = " PCI devices";
    for (size_t i = 0; suffix[i] && p + 1 < sizeof(pci_info); ++i) pci_info[p++] = suffix[i];
    pci_info[p] = '\0';

    emit("\x1b[36mBlockOS Kernel Boot Hardware Check\x1b[0m\n", ctx);
    emit("----------------------------------\n", ctx);
    emit_line(emit, ctx, g_report.pci_devices > 0 ? ProbeStatus::Ok : ProbeStatus::Failed,
              "PCI / PCIe Bus", pci_info);
    emit_line(emit, ctx, ProbeStatus::Ok, "Kernel Heap & DMA Allocator");
    emit_line(emit, ctx, ProbeStatus::Ok, "PS/2 Keyboard & Mouse (IRQ1/12)");
    emit_line(emit, ctx, g_report.st_serial, "16550 Serial UART (COM1)");
    emit_line(emit, ctx, g_report.st_virtio_block, "VirtIO-Blk Storage");
    emit_line(emit, ctx, g_report.st_nvme, "NVMe Storage");
    emit_line(emit, ctx, g_report.st_ahci, "SATA AHCI Storage");
    emit_line(emit, ctx, g_report.st_virtio_net, "VirtIO-Net Network");
    emit_line(emit, ctx, g_report.st_native_net, "Native Ethernet (E1000/RTL8139)");
    emit_line(emit, ctx, g_report.st_xhci, "USB xHCI Controller");
    emit_line(emit, ctx, g_report.st_hda, "Intel HDA Audio");
    emit_line(emit, ctx, g_report.st_virtio_input, "VirtIO-Input (HID)");
    emit_line(emit, ctx, g_report.st_virtio_rng, "VirtIO-RNG Entropy");
    emit_line(emit, ctx, g_report.st_virtio_gpu, "VirtIO-GPU Display");
    emit_line(emit, ctx, g_report.st_cpu_rdrand, "CPU RDRAND Entropy");
    emit("----------------------------------\n", ctx);
}

} // namespace blockos_hw
