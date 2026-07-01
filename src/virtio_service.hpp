#pragma once

#include <stdint.h>

namespace virtio_service {
    // Perform one poll iteration: reclaim TX buffers, drain input FIFO, and other periodic tasks.
    void poll_once();
    // Return a monotonically increasing tick counter (incremented each poll)
    uint64_t now_ticks();
}
