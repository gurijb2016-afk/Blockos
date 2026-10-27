#include "virtio_pci.hpp"
#include "pci.hpp"
#include "device_manager.hpp"

extern "C" {
#include <efi.h>
#include <efilib.h>
}

#include <string.h>

namespace virtio_pci {
namespace {

constexpr uint16_t VIRTIO_VENDOR = 0x1AF4;
constexpr uint32_t MAX_VIRTIO_DEVS = 32;

static DeviceInfo g_devices[MAX_VIRTIO_DEVS];
static uint32_t g_device_count = 0;

static const char* identify_type(uint16_t device_id)
{
    switch (device_id) {
        case 0x1000: case 0x1041: return "Network";
        case 0x1001: case 0x1042: return "Block";
        case 0x1002: case 0x1045: return "Balloon";
        case 0x1003: case 0x1043: return "Console";
        case 0x1005: case 0x1044: return "Entropy/RNG";
        case 0x1010: case 0x1050: return "GPU";
        case 0x1012: case 0x1052: return "Input";
        case 0x1013: case 0x1053: return "Socket";
        default: return "VirtIO Device";
    }
}

static DeviceType device_type_for_id(uint16_t device_id)
{
    switch (device_id) {
        case 0x1000: case 0x1041: return DEV_TYPE_NETWORK;
        case 0x1001: case 0x1042: return DEV_TYPE_STORAGE;
        case 0x1010: case 0x1050: return DEV_TYPE_GRAPHICS;
        case 0x1012: case 0x1052: return DEV_TYPE_INPUT;
        default: return DEV_TYPE_UNKNOWN;
    }
}

} // namespace

void scan()
{
    g_device_count = 0;
    Print((CHAR16*)L"virtio-pci: scanning PCI bus for VirtIO hardware...\r\n");

    for (uint32_t bus = 0; bus < 256; ++bus) {
        for (uint32_t slot = 0; slot < 32; ++slot) {
            const uint16_t vendor0 = pci_cfg_read16(static_cast<uint8_t>(bus), static_cast<uint8_t>(slot), 0, 0x00);
            if (vendor0 == 0xFFFF)
                continue;

            const uint8_t header = pci_cfg_read8(static_cast<uint8_t>(bus), static_cast<uint8_t>(slot), 0, 0x0E);
            const uint8_t funcs = (header & 0x80) ? 8 : 1;

            for (uint32_t func = 0; func < funcs; ++func) {
                const uint8_t b = static_cast<uint8_t>(bus);
                const uint8_t s = static_cast<uint8_t>(slot);
                const uint8_t f = static_cast<uint8_t>(func);

                const uint16_t vendor = pci_cfg_read16(b, s, f, 0x00);
                const uint16_t device = pci_cfg_read16(b, s, f, 0x02);

                if (vendor != VIRTIO_VENDOR)
                    continue;

                if (g_device_count >= MAX_VIRTIO_DEVS)
                    break;

                const uint64_t bar0 = pci_read_bar(b, s, f, 0);
                const bool is_modern = (device >= 0x1040 && device <= 0x107F);
                const char* type_name = identify_type(device);

                DeviceInfo& info = g_devices[g_device_count++];
                info.vendor = vendor;
                info.device = device;
                info.bus = b;
                info.slot = s;
                info.func = f;
                info.bar0 = bar0;
                info.is_modern = is_modern;
                info.type_name = type_name;

                const uint8_t class_id = pci_cfg_read8(b, s, f, 0x0B);
                const uint8_t subclass = pci_cfg_read8(b, s, f, 0x0A);
                const uint8_t prog_if  = pci_cfg_read8(b, s, f, 0x09);
                const uint8_t irq      = pci_cfg_read8(b, s, f, 0x3C);

                // Enable bus master and memory/IO access
                uint16_t cmd = pci_cfg_read16(b, s, f, 0x04);
                cmd |= 0x0007;
                pci_cfg_write16(b, s, f, 0x04, cmd);

                // Register with BlockOS device manager
                hardware_center.register_pci_device(
                    type_name,
                    device_type_for_id(device),
                    b, s, f,
                    vendor, device,
                    class_id, subclass, prog_if,
                    bar0, 0, irq
                );

                Print((CHAR16*)L"virtio-pci: [%02x:%02x.%x] %a (%a, devID=0x%04x, BAR0=0x%lx)\r\n",
                      (UINTN)b, (UINTN)s, (UINTN)f,
                      type_name, is_modern ? "modern" : "legacy",
                      (UINTN)device, (UINTN)bar0);
            }
        }
    }

    Print((CHAR16*)L"virtio-pci: scan complete (%u devices discovered)\r\n", (UINTN)g_device_count);
}

uint32_t count()
{
    return g_device_count;
}

const DeviceInfo* get(uint32_t index)
{
    if (index >= g_device_count)
        return nullptr;
    return &g_devices[index];
}

} // namespace virtio_pci
