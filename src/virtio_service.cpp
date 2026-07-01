#include "virtio_service.hpp"
#include "virtio_net_driver.hpp"
#include "virtio_input.hpp"
#include <efi.h>

void virtio_service::poll_once() {
    // Reclaim completed TX buffers
    if (virtio_net::is_available()) virtio_net::reclaim_tx();

    // Drain input FIFO (consume bytes)
    // We call read_byte_nonblocking repeatedly until empty
    if (true) {
        while (true) {
            int8_t b = virtio_input::read_byte_nonblocking();
            if (b == INT8_MIN) break;
            // For now, drop the byte or later push into event system
            (void)b;
        }
    }
}
