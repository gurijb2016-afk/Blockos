#include "virtqueue_ops.hpp"
#include <string.h>

VirtQueueView virtqueue_ops::view_from_mem(void* mem, uint32_t qsize) {
    VirtQueueView v;
    uint8_t* p = (uint8_t*)mem;
    size_t desc_sz = sizeof(VirtqDesc) * qsize;
    v.desc = (VirtqDesc*)p; p += desc_sz;
    size_t avail_hdr = sizeof(VirtqAvail);
    // avail ring size = avail_hdr + 2 bytes per ring entry (uint16_t)
    v.avail = (VirtqAvail*)p; p += (avail_hdr + sizeof(uint16_t) * qsize);
    size_t used_hdr = sizeof(VirtqUsed);
    v.used = (VirtqUsed*)p; p += (used_hdr + sizeof(VirtqUsedElem) * qsize);
    v.size = qsize;
    v.last_used_idx = 0;
    return v;
}

void virtqueue_ops::init_rings(VirtQueueView* v) {
    if (!v) return;
    // zero descriptors and rings
    memset(v->desc, 0, sizeof(VirtqDesc) * v->size);
    v->avail->flags = 0;
    v->avail->idx = 0;
    for (uint32_t i = 0; i < v->size; ++i) v->avail->ring[i] = 0;
    v->used->flags = 0;
    v->used->idx = 0;
    // zero used ring elements
    for (uint32_t i = 0; i < v->size; ++i) { v->used->ring[i].id = 0; v->used->ring[i].len = 0; }
}

void virtqueue_ops::set_descriptor(VirtQueueView* v, uint32_t idx, uint64_t addr, uint32_t len, uint16_t flags, uint16_t next) {
    if (!v || idx >= v->size) return;
    v->desc[idx].addr = addr;
    v->desc[idx].len = len;
    v->desc[idx].flags = flags;
    v->desc[idx].next = next;
}

void virtqueue_ops::submit_descriptor(VirtQueueView* v, uint32_t idx) {
    if (!v) return;
    uint16_t avail_idx = v->avail->idx % v->size;
    v->avail->ring[avail_idx] = (uint16_t)idx;
    // memory barrier would be required on SMP; here single-threaded
    v->avail->idx++;
}

bool virtqueue_ops::try_dequeue_used(VirtQueueView* v, uint32_t* id_out, uint32_t* len_out) {
    if (!v) return false;
    uint16_t cur = v->used->idx;
    if (v->last_used_idx == cur) return false;
    uint16_t idx = v->last_used_idx % v->size;
    if (id_out) *id_out = v->used->ring[idx].id;
    if (len_out) *len_out = v->used->ring[idx].len;
    v->last_used_idx++;
    return true;
}
