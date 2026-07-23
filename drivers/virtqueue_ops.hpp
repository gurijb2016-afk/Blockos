#pragma once
#include <cstdint>
#include "virtqueue.hpp"

struct VirtQueueView {
    VirtqDesc* desc;
    VirtqAvail* avail;
    VirtqUsed* used;
    uint32_t size;
    uint16_t last_used_idx;
};

namespace virtqueue_ops {
    // Create a view into a virtqueue memory region previously allocated by alloc_virtqueue
    VirtQueueView view_from_mem(void* mem, uint32_t qsize);

    // Initialize avail/used rings (zero and prepare)
    void init_rings(VirtQueueView* v);

    // Enqueue a single buffer at descriptor index 'idx' with given addr/len/flags
    void set_descriptor(VirtQueueView* v, uint32_t idx, uint64_t addr, uint32_t len, uint16_t flags, uint16_t next);

    // Submit descriptor 'idx' by adding to avail ring
    void submit_descriptor(VirtQueueView* v, uint32_t idx);

    // Try to dequeue one used element. Returns true if an element was dequeued and fills id and len.
    bool try_dequeue_used(VirtQueueView* v, uint32_t* id_out, uint32_t* len_out);
}
