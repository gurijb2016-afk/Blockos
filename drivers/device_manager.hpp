#pragma once

#include <stdint.h>
#include <stddef.h>

#define MAX_DEVICES 64

enum DeviceType
{
    DEV_TYPE_UNKNOWN = 0,
    DEV_TYPE_STORAGE,
    DEV_TYPE_NETWORK,
    DEV_TYPE_GRAPHICS,
    DEV_TYPE_INPUT,
    DEV_TYPE_USB,
    DEV_TYPE_AUDIO,
    DEV_TYPE_PCI
};

struct DeviceDescriptor
{
    uint32_t id;

    char name[32];

    DeviceType type;

    uint64_t io_base_addr;

    uint8_t irq_vector;

    uint16_t pci_vendor_id;
    uint16_t pci_device_id;

    uint8_t pci_bus;
    uint8_t pci_slot;
    uint8_t pci_func;

    bool is_present;
    bool is_ready;
};

class DeviceManager
{
private:
    DeviceDescriptor device_list[MAX_DEVICES];
    uint32_t total_devices;

    void clear_device(
        DeviceDescriptor& device
    );

    void local_strcpy(
        char* dest,
        const char* src,
        size_t max_len
    );

public:
    DeviceManager();

    bool register_device(
        const char* name,
        DeviceType type,
        uint64_t io_base,
        uint8_t irq
    );

    bool register_pci_device(
        const char* name,
        DeviceType type,
        uint8_t bus,
        uint8_t slot,
        uint8_t func,
        uint16_t vendor,
        uint16_t device,
        uint64_t io_base,
        uint8_t irq
    );

    DeviceDescriptor* find_device_by_type(
        DeviceType type
    );

    DeviceDescriptor* find_device(
        const char* name
    );

    const DeviceDescriptor* devices(
        uint32_t* count
    ) const;

    uint32_t count() const;

    bool set_ready(
        uint32_t id,
        bool ready
    );

    void initialize_all_hardware();

    void show_device_status_report();
};

extern DeviceManager hardware_center;
