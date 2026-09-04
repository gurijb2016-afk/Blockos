#pragma once

#include <stdint.h>
#include <stddef.h>

#define MAX_DEVICES 64
#define MAX_DRIVERS 64

enum DeviceType {
    DEV_TYPE_UNKNOWN = 0,
    DEV_TYPE_STORAGE,
    DEV_TYPE_NETWORK,
    DEV_TYPE_GRAPHICS,
    DEV_TYPE_INPUT,
    DEV_TYPE_USB,
    DEV_TYPE_AUDIO,
    DEV_TYPE_PCI
};

enum DeviceState {
    DEVICE_EMPTY = 0,
    DEVICE_REGISTERED,
    DEVICE_INITIALIZING,
    DEVICE_READY,
    DEVICE_FAILED
};

struct DeviceDescriptor {
    uint32_t id;

    char name[64];
    char driver[64];

    DeviceType type;
    DeviceState state;

    uint64_t io_base_addr;
    uint64_t mmio_base;
    uint64_t mmio_size;

    uint8_t irq_vector;

    uint8_t pci_bus;
    uint8_t pci_slot;
    uint8_t pci_func;

    uint16_t pci_vendor_id;
    uint16_t pci_device_id;

    uint8_t pci_class;
    uint8_t pci_subclass;
    uint8_t pci_prog_if;

    bool present;
};

typedef bool (*DriverProbeFn)(DeviceDescriptor* device);
typedef bool (*DriverInitFn)(DeviceDescriptor* device);
typedef void (*DriverRemoveFn)(DeviceDescriptor* device);

struct DriverDescriptor {
    char name[64];

    DeviceType type;

    uint16_t vendor_id;
    uint16_t device_id;

    uint8_t class_id;
    uint8_t subclass;
    uint8_t prog_if;

    DriverProbeFn probe;
    DriverInitFn init;
    DriverRemoveFn remove;

    bool registered;
};

class DeviceManager {
private:
    DeviceDescriptor device_list[MAX_DEVICES];
    DriverDescriptor driver_list[MAX_DRIVERS];

    uint32_t total_devices;
    uint32_t total_drivers;

    void local_strcpy(char* dst, const char* src, size_t max_len);

    bool driver_matches(
        const DriverDescriptor& driver,
        const DeviceDescriptor& device
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
        uint8_t class_id,
        uint8_t subclass,
        uint8_t prog_if,
        uint64_t mmio_base,
        uint64_t mmio_size,
        uint8_t irq
    );

    bool register_driver(
        const char* name,
        DeviceType type,
        uint16_t vendor_id,
        uint16_t device_id,
        uint8_t class_id,
        uint8_t subclass,
        uint8_t prog_if,
        DriverProbeFn probe,
        DriverInitFn init,
        DriverRemoveFn remove
    );

    bool bind_device(DeviceDescriptor* device);
    uint32_t bind_all_devices();

    bool initialize_device(DeviceDescriptor* device);
    uint32_t initialize_all_hardware();

    bool remove_device(DeviceDescriptor* device);

    DeviceDescriptor* find_device_by_id(uint32_t id);
    DeviceDescriptor* find_device_by_type(DeviceType type);
    DeviceDescriptor* find_device_by_name(const char* name);

    DriverDescriptor* find_driver_for_device(DeviceDescriptor* device);

    DriverDescriptor* find_driver_by_name(const char* name);

    void set_device_ready(DeviceDescriptor* device);
    void set_device_failed(DeviceDescriptor* device);

    uint32_t device_count() const;
    uint32_t driver_count() const;

    DeviceDescriptor* device_at(uint32_t index);
    DriverDescriptor* driver_at(uint32_t index);

    void show_device_status_report();
};

extern DeviceManager hardware_center;
