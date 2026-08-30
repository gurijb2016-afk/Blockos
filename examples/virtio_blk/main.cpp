#include "drivers/virtio_blk.hpp"
#include <stdint.h>

void example_virtio_block_io()
{
    uint8_t sector[512] = {};

    if (!virtio_blk::init())
        return;

    if (!virtio_blk::read_sector(0, sector))
        return;

    // Example only: do not overwrite a real filesystem sector in production.
    (void)virtio_blk::write_sector;
}
