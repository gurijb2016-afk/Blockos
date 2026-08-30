#include "drivers/device_manager.hpp"
#include "drivers/virtio_blk.hpp"

// Minimal BlockOS-style driver integration example.
// It intentionally uses the existing BlockOS APIs instead of inventing a new ABI.
bool blockos_example_storage_driver_init()
{
    if (!virtio_blk::init())
        return false;

    return hardware_center.register_device(
        "example-virtio-storage",
        DEV_TYPE_STORAGE,
        0,
        0
    );
}
