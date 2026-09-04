#pragma once

#include <stdint.h>

static constexpr uint32_t PCI_MAX_DEVICES = 64;

struct PciDevice
{
    uint8_t  bus;
    uint8_t  slot;
    uint8_t  func;

    uint16_t vendor_id;
    uint16_t device_id;

    uint8_t  class_id;
    uint8_t  subclass;
    uint8_t  prog_if;
    uint8_t  revision;

    uint8_t  header_type;

    uint64_t bar[6];

    bool     is_valid;
};

class PciSubsystem
{
private:
    PciDevice device_registry[PCI_MAX_DEVICES];
    uint32_t registered_count;

    uint32_t pci_config_read_dword(
        uint8_t bus,
        uint8_t slot,
        uint8_t func,
        uint8_t offset
    );

    uint16_t pci_config_read_word(
        uint8_t bus,
        uint8_t slot,
        uint8_t func,
        uint8_t offset
    );

    void clear_registry();

    void scan_function(
        uint8_t bus,
        uint8_t slot,
        uint8_t func
    );

public:
    PciSubsystem();

    void scan_all_pci_buses();

    const PciDevice* get_devices(
        uint32_t* count
    ) const;

    PciDevice* get_device_by_id(
        uint16_t vendor,
        uint16_t device
    );

    PciDevice* get_device(
        uint8_t bus,
        uint8_t slot,
        uint8_t func
    );

    uint32_t device_count() const;

    void configure_mmio_bars();

    void print_hardware_tree_to_gui(
        uint8_t* bb,
        uint32_t fb_w,
        int win_x,
        int win_y
    );
};

extern PciSubsystem pci_bus_manager;
