#include "device_manager.hpp"
#include "../fs/vfs.hpp"
#include "gui.hpp"

extern GuiEngine desktop;

DeviceManager hardware_center;

DeviceManager::DeviceManager()
    : total_devices(0),
      total_drivers(0)
{
    for (uint32_t i = 0; i < MAX_DEVICES; ++i) {
        device_list[i] = {};
        device_list[i].state = DEVICE_EMPTY;
        device_list[i].type = DEV_TYPE_UNKNOWN;
        device_list[i].present = false;
    }

    for (uint32_t i = 0; i < MAX_DRIVERS; ++i) {
        driver_list[i] = {};
        driver_list[i].vendor_id = 0xFFFF;
        driver_list[i].device_id = 0xFFFF;
        driver_list[i].class_id = 0xFF;
        driver_list[i].subclass = 0xFF;
        driver_list[i].prog_if = 0xFF;
        driver_list[i].registered = false;
    }
}

void DeviceManager::local_strcpy(
    char* dst,
    const char* src,
    size_t max_len
)
{
    if (!dst || max_len == 0)
        return;

    if (!src) {
        dst[0] = '\0';
        return;
    }

    size_t i = 0;

    while (i + 1 < max_len && src[i] != '\0') {
        dst[i] = src[i];
        ++i;
    }

    dst[i] = '\0';
}

bool DeviceManager::register_device(
    const char* name,
    DeviceType type,
    uint64_t io_base,
    uint8_t irq
)
{
    if (!name)
        return false;

    if (total_devices >= MAX_DEVICES)
        return false;

    DeviceDescriptor& dev =
        device_list[total_devices];

    dev = {};

    dev.id = total_devices + 1;

    local_strcpy(
        dev.name,
        name,
        sizeof(dev.name)
    );

    dev.driver[0] = '\0';

    dev.type = type;
    dev.state = DEVICE_REGISTERED;

    dev.io_base_addr = io_base;
    dev.mmio_base = io_base;
    dev.mmio_size = 0;

    dev.irq_vector = irq;

    dev.present = true;

    ++total_devices;

    return true;
}

bool DeviceManager::register_pci_device(
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
)
{
    if (!name)
        return false;

    if (total_devices >= MAX_DEVICES)
        return false;

    DeviceDescriptor& dev =
        device_list[total_devices];

    dev = {};

    dev.id = total_devices + 1;

    local_strcpy(
        dev.name,
        name,
        sizeof(dev.name)
    );

    dev.driver[0] = '\0';

    dev.type = type;
    dev.state = DEVICE_REGISTERED;

    dev.io_base_addr = mmio_base;
    dev.mmio_base = mmio_base;
    dev.mmio_size = mmio_size;

    dev.irq_vector = irq;

    dev.pci_bus = bus;
    dev.pci_slot = slot;
    dev.pci_func = func;

    dev.pci_vendor_id = vendor;
    dev.pci_device_id = device;

    dev.pci_class = class_id;
    dev.pci_subclass = subclass;
    dev.pci_prog_if = prog_if;

    dev.present = true;

    ++total_devices;

    return true;
}

bool DeviceManager::register_driver(
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
)
{
    if (!name)
        return false;

    if (!probe && !init)
        return false;

    if (total_drivers >= MAX_DRIVERS)
        return false;

    DriverDescriptor& drv =
        driver_list[total_drivers];

    drv = {};

    local_strcpy(
        drv.name,
        name,
        sizeof(drv.name)
    );

    drv.type = type;

    drv.vendor_id = vendor_id;
    drv.device_id = device_id;

    drv.class_id = class_id;
    drv.subclass = subclass;
    drv.prog_if = prog_if;

    drv.probe = probe;
    drv.init = init;
    drv.remove = remove;

    drv.registered = true;

    ++total_drivers;

    return true;
}

bool DeviceManager::driver_matches(
    const DriverDescriptor& driver,
    const DeviceDescriptor& device
)
{
    if (!driver.registered)
        return false;

    if (driver.type != DEV_TYPE_UNKNOWN &&
        driver.type != device.type)
        return false;

    if (driver.vendor_id != 0xFFFF &&
        driver.vendor_id != device.pci_vendor_id)
        return false;

    if (driver.device_id != 0xFFFF &&
        driver.device_id != device.pci_device_id)
        return false;

    if (driver.class_id != 0xFF &&
        driver.class_id != device.pci_class)
        return false;

    if (driver.subclass != 0xFF &&
        driver.subclass != device.pci_subclass)
        return false;

    if (driver.prog_if != 0xFF &&
        driver.prog_if != device.pci_prog_if)
        return false;

    return true;
}

DriverDescriptor* DeviceManager::find_driver_for_device(
    DeviceDescriptor* device
)
{
    if (!device)
        return nullptr;

    for (uint32_t i = 0;
         i < total_drivers;
         ++i)
    {
        if (driver_matches(
                driver_list[i],
                *device))
        {
            return &driver_list[i];
        }
    }

    return nullptr;
}

DriverDescriptor* DeviceManager::find_driver_by_name(
    const char* name
)
{
    if (!name)
        return nullptr;

    for (uint32_t i = 0;
         i < total_drivers;
         ++i)
    {
        const char* a =
            driver_list[i].name;

        const char* b = name;

        size_t p = 0;

        while (a[p] &&
               b[p] &&
               a[p] == b[p])
        {
            ++p;
        }

        if (a[p] == '\0' &&
            b[p] == '\0')
        {
            return &driver_list[i];
        }
    }

    return nullptr;
}

static bool register_vfs_device(
    DeviceDescriptor* dev
)
{
    if (!dev)
        return false;

    if (!dev->present)
        return false;

    if (dev->state != DEVICE_READY)
        return false;

    vfs::initialize_devices();

    uint64_t base =
        dev->mmio_base;

    uint64_t size =
        dev->mmio_size;

    uint8_t irq =
        dev->irq_vector;

    switch (dev->type)
    {
        case DEV_TYPE_STORAGE:
            return vfs::register_disk(
                base,
                size,
                irq,
                dev->pci_bus,
                dev->pci_slot,
                dev->pci_func,
                dev->pci_vendor_id,
                dev->pci_device_id
            );

        case DEV_TYPE_NETWORK:
            return vfs::register_network_device(
                base,
                size,
                irq,
                dev->pci_bus,
                dev->pci_slot,
                dev->pci_func,
                dev->pci_vendor_id,
                dev->pci_device_id
            );

        case DEV_TYPE_USB:
            return vfs::register_usb_device(
                base,
                size,
                irq,
                dev->pci_bus,
                dev->pci_slot,
                dev->pci_func,
                dev->pci_vendor_id,
                dev->pci_device_id
            );

        case DEV_TYPE_GRAPHICS:
            return vfs::register_gpu_device(
                base,
                size,
                irq,
                dev->pci_bus,
                dev->pci_slot,
                dev->pci_func,
                dev->pci_vendor_id,
                dev->pci_device_id
            );

        case DEV_TYPE_INPUT:
            return vfs::register_input_device(
                base,
                size,
                irq,
                dev->pci_bus,
                dev->pci_slot,
                dev->pci_func,
                dev->pci_vendor_id,
                dev->pci_device_id
            );

        case DEV_TYPE_AUDIO:
            return vfs::register_audio_device(
                base,
                size,
                irq,
                dev->pci_bus,
                dev->pci_slot,
                dev->pci_func,
                dev->pci_vendor_id,
                dev->pci_device_id
            );

        default:
            return false;
    }
}

bool DeviceManager::bind_device(
    DeviceDescriptor* device
)
{
    if (!device)
        return false;

    if (!device->present)
        return false;

    if (device->state == DEVICE_READY)
        return true;

    DriverDescriptor* driver =
        find_driver_for_device(device);

    if (!driver) {
        device->state =
            DEVICE_FAILED;

        return false;
    }

    device->state =
        DEVICE_INITIALIZING;

    if (driver->probe) {
        if (!driver->probe(device)) {
            device->state =
                DEVICE_FAILED;

            return false;
        }
    }

    if (driver->init) {
        if (!driver->init(device)) {
            device->state =
                DEVICE_FAILED;

            return false;
        }
    }

    local_strcpy(
        device->driver,
        driver->name,
        sizeof(device->driver)
    );

    device->state =
        DEVICE_READY;

    register_vfs_device(device);

    return true;
}

uint32_t DeviceManager::bind_all_devices()
{
    uint32_t bound = 0;

    for (uint32_t i = 0;
         i < total_devices;
         ++i)
    {
        if (bind_device(
                &device_list[i]))
        {
            ++bound;
        }
    }

    return bound;
}

bool DeviceManager::initialize_device(
    DeviceDescriptor* device
)
{
    return bind_device(device);
}

uint32_t DeviceManager::initialize_all_hardware()
{
    vfs::initialize_devices();

    return bind_all_devices();
}

bool DeviceManager::remove_device(
    DeviceDescriptor* device
)
{
    if (!device)
        return false;

    DriverDescriptor* driver =
        find_driver_for_device(device);

    if (driver && driver->remove)
        driver->remove(device);

    device->state =
        DEVICE_REGISTERED;

    device->driver[0] =
        '\0';

    return true;
}

DeviceDescriptor* DeviceManager::find_device_by_id(
    uint32_t id
)
{
    for (uint32_t i = 0;
         i < total_devices;
         ++i)
    {
        if (device_list[i].id == id)
            return &device_list[i];
    }

    return nullptr;
}

DeviceDescriptor* DeviceManager::find_device_by_type(
    DeviceType type
)
{
    for (uint32_t i = 0;
         i < total_devices;
         ++i)
    {
        if (device_list[i].present &&
            device_list[i].type == type)
        {
            return &device_list[i];
        }
    }

    return nullptr;
}

DeviceDescriptor* DeviceManager::find_device_by_name(
    const char* name
)
{
    if (!name)
        return nullptr;

    for (uint32_t i = 0;
         i < total_devices;
         ++i)
    {
        const char* a =
            device_list[i].name;

        const char* b = name;

        size_t p = 0;

        while (a[p] &&
               b[p] &&
               a[p] == b[p])
        {
            ++p;
        }

        if (a[p] == '\0' &&
            b[p] == '\0')
        {
            return &device_list[i];
        }
    }

    return nullptr;
}

void DeviceManager::set_device_ready(
    DeviceDescriptor* device
)
{
    if (device)
        device->state =
            DEVICE_READY;
}

void DeviceManager::set_device_failed(
    DeviceDescriptor* device
)
{
    if (device)
        device->state =
            DEVICE_FAILED;
}

uint32_t DeviceManager::device_count() const
{
    return total_devices;
}

uint32_t DeviceManager::driver_count() const
{
    return total_drivers;
}

DeviceDescriptor* DeviceManager::device_at(
    uint32_t index
)
{
    if (index >= total_devices)
        return nullptr;

    return &device_list[index];
}

DriverDescriptor* DeviceManager::driver_at(
    uint32_t index
)
{
    if (index >= total_drivers)
        return nullptr;

    return &driver_list[index];
}

void DeviceManager::show_device_status_report()
{
    for (uint32_t i = 0;
         i < total_devices;
         ++i)
    {
        uint32_t x =
            600 + i * 24;

        if (device_list[i].state ==
            DEVICE_READY)
        {
            desktop.draw_rect(
                x,
                140,
                16,
                16,
                COLOR_ARGB(
                    255,
                    0,
                    255,
                    0
                )
            );
        }
        else if (
            device_list[i].state ==
            DEVICE_FAILED)
        {
            desktop.draw_rect(
                x,
                140,
                16,
                16,
                COLOR_ARGB(
                    255,
                    255,
                    0,
                    0
                )
            );
        }
        else
        {
            desktop.draw_rect(
                x,
                140,
                16,
                16,
                COLOR_ARGB(
                    255,
                    255,
                    0,
                    0
                )
            );
        }
    }

    desktop.render();
}
