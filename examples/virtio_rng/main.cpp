#include "drivers/virtio_rng.hpp"

void example_virtio_rng()
{
    if (!virtio_rng::init())
        return;

    uint32_t val = virtio_rng::get_random_u32();
    (void)val;

    uint8_t buffer[64]{};
    virtio_rng::get_random_bytes(buffer, sizeof(buffer));
}
