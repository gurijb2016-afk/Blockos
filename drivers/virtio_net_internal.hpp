#pragma once

#include <stdint.h>
#include <stddef.h>

#include "virtqueue_ops.hpp"

namespace virtio_net {

struct TxSlot {
    void* buf;
    uint64_t submit_tick;
};

// Közös driver állapot
extern bool g_ready;

extern VirtQueueView g_tx_vq;
extern VirtQueueView g_rx_vq;

extern TxSlot g_tx_slots[256];

// TX buffer pool
void tx_pool_push(void* buf);

}
