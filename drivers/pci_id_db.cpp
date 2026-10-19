#include "pci_id_db.hpp"

namespace pci_id_db {
namespace {
struct VendorName { uint16_t id; const char* name; };
struct DeviceName { uint16_t vendor, device; const char* name; };

static const VendorName vendors[] = {
    {0x8086, "Intel"}, {0x1022, "AMD"}, {0x1002, "AMD/ATI"},
    {0x10DE, "NVIDIA"}, {0x10EC, "Realtek"}, {0x14E4, "Broadcom"},
    {0x1AF4, "VirtIO"}, {0x15AD, "VMware"}, {0x1B36, "QEMU/Red Hat"},
    {0x1234, "Bochs/QEMU"}, {0x80EE, "VirtualBox"}, {0x144D, "Samsung"},
    {0x1987, "Phison"}, {0x1C5C, "SK hynix"}, {0x103C, "HP"},
    {0x17AA, "Lenovo"}, {0x1043, "ASUS"}, {0x1028, "Dell"},
    {0x1462, "MSI"}, {0x1B4B, "Marvell"}, {0x1D0F, "Amazon"},
    {0x5853, "Xen"}, {0x1D17, "Zhaoxin"}
};

static const DeviceName devices[] = {
    {0x8086, 0x100E, "Intel 82540EM Gigabit Ethernet"},
    {0x8086, 0x100F, "Intel 82545EM Gigabit Ethernet"},
    {0x8086, 0x1004, "Intel PRO/1000 Ethernet family"},
    {0x8086, 0x1005, "Intel PRO/1000 Ethernet family"},
    {0x8086, 0x10D3, "Intel 82574L Ethernet"},
    {0x10EC, 0x8139, "Realtek RTL8139 Ethernet"},
    {0x10EC, 0x8138, "Realtek RTL813x Ethernet"},
    {0x10EC, 0x8100, "Realtek RTL8100 Ethernet"},
    {0x1AF4, 0x1000, "VirtIO network (transitional)"},
    {0x1AF4, 0x1001, "VirtIO block (transitional)"},
    {0x1AF4, 0x1041, "VirtIO network (modern)"},
    {0x1AF4, 0x1042, "VirtIO block (modern)"},
    {0x1AF4, 0x1002, "VirtIO memory balloon"},
    {0x1AF4, 0x1003, "VirtIO console"},
    {0x1AF4, 0x1005, "VirtIO random number generator"},
    {0x1234, 0x1111, "Bochs/QEMU VGA adapter"},
    {0x1B36, 0x000D, "QEMU XHCI controller"},
    {0x1B36, 0x0002, "QEMU PCI bridge"},
    {0x15AD, 0x0405, "VMware SVGA adapter"},
    {0x80EE, 0xBEEF, "VirtualBox graphics adapter"}
};
}

const char* vendor_name(uint16_t vendor)
{
    for (const auto& v : vendors)
        if (v.id == vendor) return v.name;
    return "Unknown vendor";
}

const char* device_name(uint16_t vendor, uint16_t device)
{
    for (const auto& d : devices)
        if (d.vendor == vendor && d.device == device) return d.name;
    return nullptr;
}

const char* class_name(uint8_t c)
{
    switch (c) {
        case 0x00: return "Unclassified";
        case 0x01: return "Mass storage";
        case 0x02: return "Network controller";
        case 0x03: return "Display controller";
        case 0x04: return "Multimedia controller";
        case 0x05: return "Memory controller";
        case 0x06: return "Bridge";
        case 0x07: return "Communication controller";
        case 0x08: return "System peripheral";
        case 0x09: return "Input controller";
        case 0x0A: return "Docking station";
        case 0x0B: return "Processor";
        case 0x0C: return "Serial bus controller";
        case 0x0D: return "Wireless controller";
        case 0x0E: return "Intelligent I/O";
        case 0x0F: return "Satellite controller";
        case 0x10: return "Encryption controller";
        case 0x11: return "Signal processing";
        case 0x12: return "Processing accelerator";
        case 0x13: return "Non-essential instrumentation";
        case 0x40: return "Co-processor";
        case 0xFF: return "Vendor-specific";
        default: return "Unknown PCI class";
    }
}

const char* subclass_name(uint8_t c, uint8_t s, uint8_t p)
{
    if (c == 0x01) {
        switch (s) {
            case 0x00: return "SCSI storage";
            case 0x01: return "IDE storage";
            case 0x04: return "RAID controller";
            case 0x06: return p == 0x01 ? "SATA AHCI controller" : "SATA controller";
            case 0x07: return "SAS controller";
            case 0x08: return p == 0x02 ? "NVM Express controller" : "Non-volatile memory";
            case 0x80: return "Other storage controller";
            default: break;
        }
    } else if (c == 0x02) {
        if (s == 0x00) return "Ethernet controller";
        if (s == 0x80) return "Other network controller";
    } else if (c == 0x03) {
        if (s == 0x00) return "VGA-compatible controller";
        if (s == 0x02) return "3D controller";
    } else if (c == 0x04) {
        if (s == 0x01) return "Multimedia video";
        if (s == 0x02) return "Audio controller";
        if (s == 0x03) return "Audio device / HDA family";
    } else if (c == 0x06) {
        if (s == 0x00) return "Host bridge";
        if (s == 0x01) return "ISA bridge";
        if (s == 0x04) return "PCI-to-PCI bridge";
    } else if (c == 0x0C) {
        if (s == 0x03) {
            if (p == 0x00) return "UHCI USB controller";
            if (p == 0x10) return "OHCI USB controller";
            if (p == 0x20) return "EHCI USB controller";
            if (p == 0x30) return "xHCI USB controller";
            return "USB controller";
        }
        if (s == 0x05) return "SMBus controller";
        if (s == 0x80) return "Other serial bus controller";
    } else if (c == 0x0D) {
        if (s == 0x11) return "Bluetooth controller";
        if (s == 0x20) return "Wireless controller";
    }
    return "PCI subclass not in small built-in table";
}

const char* driver_hint(uint16_t vendor, uint16_t device,
                        uint8_t c, uint8_t s, uint8_t p)
{
    if (vendor == 0x1AF4) {
        if (device == 0x1000 || device == 0x1041) return "VirtIO network driver (probe required)";
        if (device == 0x1001 || device == 0x1042) return "VirtIO block driver (probe required)";
    }
    if (c == 0x01 && s == 0x08 && p == 0x02) return "NVMe driver (probe required)";
    if (c == 0x01 && s == 0x06 && p == 0x01) return "AHCI driver (probe required)";
    if (c == 0x0C && s == 0x03 && p == 0x30) return "xHCI driver (probe required)";
    if (c == 0x04 && s == 0x03) return "HDA audio driver (probe required)";
    if (vendor == 0x8086 && c == 0x02) {
        switch (device) {
            case 0x100E: case 0x100F: case 0x1004: case 0x1005:
            case 0x100C: case 0x1015: case 0x1016: case 0x1017:
            case 0x101E: case 0x1026: case 0x1049: case 0x104A:
            case 0x104B: case 0x104D: case 0x105E: case 0x1075:
            case 0x1076: case 0x1077: case 0x1078: case 0x1079:
            case 0x107A: case 0x107B: case 0x107C: case 0x107D:
            case 0x107E: case 0x107F: case 0x108A: case 0x108B:
            case 0x10D3: case 0x10EA:
                return "Intel E1000 driver (probe required)";
            default: break;
        }
    }
    if (vendor == 0x10EC && (device == 0x8139 || device == 0x8138 || device == 0x8100))
        return "Realtek RTL813x driver (probe required)";
    if (vendor == 0x14E4 && c == 0x02) {
        switch (device) {
            case 0x1644: case 0x1645: case 0x1646: case 0x1647:
            case 0x1648: case 0x1649: case 0x164A: case 0x164B:
            case 0x1653: case 0x1654: case 0x1657: case 0x165D:
            case 0x165E: case 0x165F: case 0x1668: case 0x1669:
            case 0x1677: case 0x1678: case 0x1679: case 0x167A:
            case 0x1680: case 0x1681: case 0x1682: case 0x1684:
            case 0x1686: case 0x1687: case 0x1690: case 0x1691:
            case 0x1692: case 0x1693: case 0x1694: case 0x16A6:
            case 0x16A7: case 0x16A8: case 0x16A9: case 0x16AA:
            case 0x16B0: case 0x16B1: case 0x16B2: case 0x16B4:
            case 0x16B5: case 0x16B6: case 0x16B7: case 0x16B9:
            case 0x16BC: case 0x16BD: case 0x16BE: case 0x16BF:
                return "Broadcom network driver (probe required)";
            default: break;
        }
    }
    return nullptr;
}

bool has_driver_hint(uint16_t vendor, uint16_t device,
                     uint8_t c, uint8_t s, uint8_t p)
{
    return driver_hint(vendor, device, c, s, p) != nullptr;
}
}
