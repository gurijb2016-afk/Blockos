#include "virtio_blk_full.hpp"
#include "virtio_common.hpp"
#include "virtqueue.hpp"
#include "virtqueue_ops.hpp"
#include "virtio_notify.hpp"
#include "dma.hpp"
#include <efi.h>
#include <efilib.h>
#include <string.h>

static bool blk_ready = false;
static VirtQueueView g_blk_vq;
static void* g_blk_mem = nullptr;
static uint32_t g_blk_qsize = 8;
static virtio_common::DeviceHandle g_blk_handle;

#pragma pack(push,1)
struct VirtioBlkReqFull {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
};
#pragma pack(pop)

enum { BLK_T_IN = 0, BLK_T_OUT = 1, BLK_T_FLUSH = 4 };

bool virtio_blk_full::init() {
    if (blk_ready) return true;
    if (!virtio_common::probe_device(virtio_common::DeviceType::BLOCK, &g_blk_handle)) return false;
    if (!virtio_common::device_init(&g_blk_handle)) return false;

    size_t perq = (sizeof(VirtqDesc) * g_blk_qsize) + (sizeof(uint16_t) * g_blk_qsize) + (sizeof(VirtqUsedElem) * g_blk_qsize) + 4096;
    g_blk_mem = dma::alloc(perq, 4096);
    if (!g_blk_mem) return false;
    uint32_t desc_count = 0;
    VirtqDesc* desc = virtqueue::alloc_virtqueue(g_blk_mem, g_blk_qsize, &desc_count);
    if (!desc) return false;
    g_blk_vq = virtqueue_ops::view_from_mem(g_blk_mem, g_blk_qsize);
    virtqueue_ops::init_rings(&g_blk_vq);

    // Try modern queue programming
    virtio_common::program_modern_queue_addr(&g_blk_handle, 0, (uint64_t)(UINTN)g_blk_vq.desc, (uint64_t)(UINTN)g_blk_vq.avail, (uint64_t)(UINTN)g_blk_vq.used);
    // Fallback to legacy PFN
    extern bool program_queue_pfn(const virtio_common::DeviceHandle* h, uint16_t queue_sel, void* pfn_mem);
    program_queue_pfn(&g_blk_handle, 0, g_blk_mem);

    blk_ready = true;
    Print(L"virtio-blk-full: initialized\n");
    return true;
}

static bool submit_block_request_chain_generic(void* header_dma, void* data_dma, uint32_t data_len, void* status_dma) {
    const uint16_t VIRTQ_DESC_F_WRITE = 2;
    // find a free descriptor slot (use 0..size-1)
    uint32_t start_idx = UINT32_MAX;
    for (uint32_t i = 0; i < g_blk_vq.size; ++i) {
        if (g_blk_vq.desc[i].len == 0) { start_idx = i; break; }
    }
    if (start_idx == UINT32_MAX) return false;

    // Use three consecutive descriptors wrapping if necessary
    uint32_t idx0 = start_idx;
    uint32_t idx1 = (idx0 + 1) % g_blk_vq.size;
    uint32_t idx2 = (idx1 + 1) % g_blk_vq.size;

    virtqueue_ops::set_descriptor(&g_blk_vq, idx0, (uint64_t)(UINTN)header_dma, sizeof(VirtioBlkReqFull), 0, (uint16_t)idx1);
    uint16_t flags = (data_len > 0 && data_dma) ? VIRTQ_DESC_F_WRITE : VIRTQ_DESC_F_WRITE; // for IN, device writes; for OUT, device reads
    // For OUT (write), flags should be 0 (device reads); for IN, flags=WRITE
    // We detect by checking header->type later; but caller should set flags accordingly. Here, caller prepared data_dma appropriately.
    virtqueue_ops::set_descriptor(&g_blk_vq, idx1, (uint64_t)(UINTN)data_dma, data_len, flags, (uint16_t)idx2);
    virtqueue_ops::set_descriptor(&g_blk_vq, idx2, (uint64_t)(UINTN)status_dma, 1, VIRTQ_DESC_F_WRITE, 0);

    virtqueue_ops::submit_descriptor(&g_blk_vq, idx0);
    virtio_notify::notify(&g_blk_handle, 0);

    // wait for completion with timeout
    uint32_t id; uint32_t len;
    for (uint32_t i = 0; i < 5000000; ++i) {
        if (virtqueue_ops::try_dequeue_used(&g_blk_vq, &id, &len)) {
            uint8_t st = *(uint8_t*)(UINTN)status_dma;
            return st == 0;
        }
    }
    return false;
}

bool virtio_blk_full::read_sector(uint64_t sector, uint8_t* out_buf) {
    return virtio_blk_full::read_sectors(sector, 1, out_buf);
}

bool virtio_blk_full::write_sector(uint64_t sector, const uint8_t* in_buf) {
    return virtio_blk_full::write_sectors(sector, 1, in_buf);
}

bool virtio_blk_full::read_sectors(uint64_t sector, uint32_t count, uint8_t* out_buf) {
    if (!blk_ready) return false;
    if (count == 0) return false;
    // Loop per-sector for simplicity
    for (uint32_t i = 0; i < count; ++i) {
        void* header = dma::alloc(sizeof(VirtioBlkReqFull), 4096);
        void* data = dma::alloc(512, 512);
        void* status = dma::alloc(1, 1);
        if (!header || !data || !status) return false;
        VirtioBlkReqFull h;
        memset(&h,0,sizeof(h));
        h.type = BLK_T_IN;
        h.ioprio = 0;
        h.sector = sector + i;
        memcpy(header, &h, sizeof(h));
        memset(status, 0, 1);
        bool ok = submit_block_request_chain_generic(header, data, 512, status);
        if (!ok) return false;
        memcpy(out_buf + i*512, data, 512);
    }
    return true;
}

bool virtio_blk_full::write_sectors(uint64_t sector, uint32_t count, const uint8_t* in_buf) {
    if (!blk_ready) return false;
    if (count == 0) return false;
    for (uint32_t i = 0; i < count; ++i) {
        void* header = dma::alloc(sizeof(VirtioBlkReqFull), 4096);
        void* data = dma::alloc(512, 512);
        void* status = dma::alloc(1, 1);
        if (!header || !data || !status) return false;
        VirtioBlkReqFull h;
        memset(&h,0,sizeof(h));
        h.type = BLK_T_OUT;
        h.ioprio = 0;
        h.sector = sector + i;
        memcpy(header, &h, sizeof(h));
        memcpy(data, in_buf + i*512, 512);
        memset(status, 0, 1);
        bool ok = submit_block_request_chain_generic(header, data, 512, status);
        if (!ok) return false;
    }
    return true;
}

bool virtio_blk_full::flush() {
    if (!blk_ready) return false;
    // send a FLUSH request with no data
    void* header = dma::alloc(sizeof(VirtioBlkReqFull), 4096);
    void* status = dma::alloc(1, 1);
    if (!header || !status) return false;
    VirtioBlkReqFull h;
    memset(&h,0,sizeof(h));
    h.type = BLK_T_FLUSH;
    h.ioprio = 0;
    h.sector = 0;
    memcpy(header, &h, sizeof(h));
    memset(status, 0, 1);
    bool ok = submit_block_request_chain_generic(header, NULL, 0, status);
    return ok;
}
