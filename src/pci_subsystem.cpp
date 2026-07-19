#include "pci_subsystem.hpp"
#include "gui.hpp"

extern GuiEngine desktop;

PciSubsystem::PciSubsystem() : registered_count(0) {
    for(int i=0; i<64; i++) device_registry[i].is_valid = false;
}

uint16_t PciSubsystem::pci_config_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((uint32_t)bus << 16) | ((uint32_t)slot << 11) |
                       ((uint32_t)func << 8) | (offset & 0xFC) | ((uint32_t)0x80000000);
    
    // Alacsony szintű assembly port-I/O parancsok a PCI vezérlőhöz [source: 1]
    asm volatile("out dx, eax" : : "a"(address), "d"((uint16_t)0xCF8));
    uint32_t tmp;
    asm volatile("in eax, dx" : "=a"(tmp) : "d"((uint16_t)0xCFC));
    return (uint16_t)((tmp >> ((offset & 2) * 8)) & 0xFFFF);
}

uint32_t PciSubsystem::pci_config_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((uint32_t)bus << 16) | ((uint32_t)slot << 11) |
                       ((uint32_t)func << 8) | (offset & 0xFC) | ((uint32_t)0x80000000);
    asm volatile("out dx, eax" : : "a"(address), "d"((uint16_t)0xCF8));
    uint32_t tmp;
    asm volatile("in eax, dx" : "=a"(tmp) : "d"((uint16_t)0xCFC));
    return tmp;
}

void PciSubsystem::scan_all_pci_buses() {
    for (uint16_t bus = 0; bus < 8; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint16_t vendor = pci_config_read_word(bus, slot, func, 0);
                if (vendor == 0xFFFF) continue; // Nincs itt eszköz

                uint16_t device = pci_config_read_word(bus, slot, func, 2);
                uint32_t class_rev = pci_config_read_dword(bus, slot, func, 8);
                
                if (registered_count < 64) {
                    PciDevice& d = device_registry[registered_count];
                    d.bus = bus; d.slot = slot; d.func = func;
                    d.vendor_id = vendor; d.device_id = device;
                    d.class_id = (class_rev >> 24) & 0xFF;
                    d.subclass = (class_rev >> 16) & 0xFF;
                    d.bar0 = pci_config_read_dword(bus, slot, func, 0x10);
                    d.bar1 = pci_config_read_dword(bus, slot, func, 0x14);
                    d.is_valid = true;
                    registered_count++;
                }
            }
        }
    }
}

void PciSubsystem::configure_mmio_bars() {
    for (uint32_t i = 0; i < registered_count; i++) {
        if (!device_registry[i].is_valid) continue;
        // Ha VirtIO eszközt találunk (Vendor ID: 0x1AF4), kényszerítjük a memóriatérképezést [source: 1]
        if (device_registry[i].vendor_id == 0x1AF4) {
            device_registry[i].bar0 |= 0x1; // MMIO I/O Space bit engedélyezés
        }
    }
}

void PciSubsystem::print_hardware_tree_to_gui(uint8_t* bb, uint32_t fb_w, int win_x, int win_y) {
    int current_y = win_y + 36;
    auto print_line = [&](const char* text, uint32_t col) {
        int cx = win_x + 15;
        while(*text) { bb_draw_char(bb, fb_w, cx, current_y, *text, col); cx += 8; text++; }
        current_y += 12;
    };

    print_line("--- BARE-METAL PCI HARDWARE EXPLORER ---", 0x0000D0FF);
    for (uint32_t i = 0; i < registered_count; i++) {
        if (current_y > win_y + 200) break;
        PciDevice& d = device_registry[i];
        
        // Dinamikus hardverjelentés szimulálása
        if (d.vendor_id == 0x1AF4 && d.device_id == 0x1001) print_line("[PCI] VirtIO Block Controller Found", 0x0000FF00);
        else if (d.vendor_id == 0x1AF4 && d.device_id == 0x1000) print_line("[PCI] VirtIO Network Adapter Found", 0x0000FF00);
        else if (d.vendor_id == 0x8086) print_line("[PCI] Intel Corp. Host Bridge Controller", 0x00FFFFFF);
        else print_line("[PCI] Generic Multi-Driver PCI Device Active", 0x00AAAAAA);
    }
}

PciDevice* PciSubsystem::get_device_by_id(uint16_t vendor, uint16_t device) {
    for (uint32_t i = 0; i < registered_count; i++) {
        if (device_registry[i].is_valid && device_registry[i].vendor_id == vendor && device_registry[i].device_id == device) {
            return &device_registry[i];
        }
    }
    return nullptr;
}

PciSubsystem pci_bus_manager;
