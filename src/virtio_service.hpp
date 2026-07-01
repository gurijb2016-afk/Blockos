#pragma once

namespace virtio_service {
    // Perform one poll iteration: reclaim TX buffers, drain input FIFO, and other periodic tasks.
    void poll_once();
}
