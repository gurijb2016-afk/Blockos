#include "virtio_service.hpp"
#include "virtio_net_driver.hpp"
#include "virtio_input.hpp"
#include <efi.h>

static uint64_t g_virtio_tick = 0;

void virtio_service::poll_once() {
    // increment global tick
    g_virtio_tick++;

    // Reclaim completed TX buffers
    if (virtio_net::is_available()) virtio_net::reclaim_tx();

    // Drain input FIFO (consume bytes)
    while (true) {
        int8_t b = virtio_input::read_byte_nonblocking();
        if (b == INT8_MIN) break;
        // For now, drop the byte or later push into event system
        (void)b;
    }
}

uint64_t virtio_service::now_ticks() {
    return g_virtio_tick;
}
