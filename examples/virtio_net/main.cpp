#include "drivers/virtio_net_driver.hpp"
#include <stdint.h>

void example_virtio_network()
{
    static const uint8_t probe[] = { 0x42, 0x4c, 0x4b, 0x4f, 0x53 };
    uint8_t rx[2048] = {};

    if (!virtio_net::init() || !virtio_net::is_available())
        return;

    (void)virtio_net::send_packet(probe, sizeof(probe));
    (void)virtio_net::receive_packet(rx, sizeof(rx));
    virtio_net::reclaim_tx();
}
