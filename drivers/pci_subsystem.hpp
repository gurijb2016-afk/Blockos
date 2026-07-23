#pragma once
#include <stdint.h>

struct PciDevice {
    uint8_t  bus;
    uint8_t  slot;
    uint8_t  func;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  class_id;
    uint8_t  subclass;
    uint32_t bar0;
    uint32_t bar1;
    bool     is_valid;
};

class PciSubsystem {
private:
    PciDevice device_registry[64];
    uint32_t  registered_count;

    uint16_t pci_config_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
    uint32_t pci_config_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);

public:
    PciSubsystem();
    void scan_all_pci_buses();
    void configure_mmio_bars();
    void print_hardware_tree_to_gui(uint8_t* bb, uint32_t fb_w, int win_x, int win_y);
    PciDevice* get_device_by_id(uint16_t vendor, uint16_t device);
};

extern PciSubsystem pci_bus_manager;
