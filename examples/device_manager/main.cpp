#include "drivers/device_manager.hpp"

// Kernel-space example: register a BlockOS device and query it by category.
void example_device_manager()
{
    hardware_center.register_device(
        "example-storage",
        DEV_TYPE_STORAGE,
        0x10000000,
        11
    );

    DeviceDescriptor* dev =
        hardware_center.find_device_by_type(DEV_TYPE_STORAGE);

    if (dev)
        hardware_center.show_device_status_report();
}
