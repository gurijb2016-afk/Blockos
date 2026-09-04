#pragma once

#include <stdint.h>
#include <stddef.h>

#define MAX_DEVICES 64
#define DRIVER_NAME_MAX 32
#define DEVICE_NAME_MAX 32

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
    DEV_STATE_EMPTY = 0,
    DEV_STATE_REGISTERED,
    DEV_STATE_INITIALIZING,
    DEV_STATE_READY,
    DEV_STATE_FAILED
};

struct DeviceDescriptor {
    uint32_t id;

    char name[DEVICE_NAME_MAX];
    char driver[DRIVER_NAME_MAX];

    DeviceType type;
    DeviceState state;

    uint64_t io_base_addr;
    uint64_t io_size;

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
    char name[DRIVER_NAME_MAX];

    DeviceType type;

    uint16_t vendor_id;
    uint16_t device_id;

    uint8_t class_code;
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
    DriverDescriptor driver_list[MAX_DEVICES];

    uint32_t total_devices;
    uint32_t total_drivers;
    uint32_t next_device_id;

    void local_strcpy(char* dest, const char* src, size_t max_len);
    bool driver_matches(const DriverDescriptor& driver,
                        const DeviceDescriptor& device) const;

public:
    DeviceManager();

    bool register_device(
        const char* name,
        DeviceType type,
        uint64_t io_base,
        uint64_t io_size,
        uint8_t irq
    );

    bool register_pci_device(
        const char* name,
        DeviceType type,
        uint8_t bus,
        uint8_t slot,
        uint8_t func,
        uint16_t vendor_id,
        uint16_t device_id,
        uint8_t class_code,
        uint8_t subclass,
        uint8_t prog_if,
        uint64_t io_base,
        uint64_t io_size,
        uint8_t irq
    );

    bool register_driver(
        const char* name,
        DeviceType type,
        uint16_t vendor_id,
        uint16_t device_id,
        uint8_t class_code,
        uint8_t subclass,
        uint8_t prog_if,
        DriverProbeFn probe,
        DriverInitFn init,
        DriverRemoveFn remove
    );

    bool bind_device(uint32_t device_id);
    void bind_all_devices();

    bool initialize_device(uint32_t device_id);
    void initialize_all_hardware();

    bool remove_device(uint32_t device_id);

    DeviceDescriptor* find_device(uint32_t device_id);
    DeviceDescriptor* find_device_by_type(DeviceType type);
    DeviceDescriptor* find_device_by_name(const char* name);

    DriverDescriptor* find_driver_for_device(DeviceDescriptor* device);

    DeviceDescriptor* devices();
    const DeviceDescriptor* devices() const;

    DriverDescriptor* drivers();
    const DriverDescriptor* drivers() const;

    uint32_t count() const;
    uint32_t driver_count() const;

    bool set_ready(uint32_t device_id);
    bool set_failed(uint32_t device_id);

    void show_device_status_report();
};

extern DeviceManager hardware_center;
