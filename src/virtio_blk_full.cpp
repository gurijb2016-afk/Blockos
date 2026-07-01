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

enum { BLK_T_IN = 0, BLK_T_OUT = 1 };

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

static bool submit_block_request_chain(void* header_dma, void* data_dma, void* status_dma) {
    const uint16_t VIRTQ_DESC_F_WRITE = 2;
    // use descriptors 0,1,2 like before but support chaining explicitly
    virtqueue_ops::set_descriptor(&g_blk_vq, 0, (uint64_t)(UINTN)header_dma, sizeof(VirtioBlkReqFull), 0, 1);
    // data buffer (may be read or write)
    // determine flags based on status byte in header
    uint16_t flags = VIRTQ_DESC_F_WRITE; // driver expects device to write data for IN
    virtqueue_ops::set_descriptor(&g_blk_vq, 1, (uint64_t)(UINTN)data_dma, 512, flags, 2);
    // status
    virtqueue_ops::set_descriptor(&g_blk_vq, 2, (uint64_t)(UINTN)status_dma, 1, VIRTQ_DESC_F_WRITE, 0);
    virtqueue_ops::submit_descriptor(&g_blk_vq, 0);
    // notify device
    virtio_notify::notify(&g_blk_handle, 0);

    // wait for completion with timeout
    uint32_t id; uint32_t len;
    for (uint32_t i = 0; i < 2000000; ++i) {
        if (virtqueue_ops::try_dequeue_used(&g_blk_vq, &id, &len)) {
            uint8_t st = *(uint8_t*)(UINTN)status_dma;
            return st == 0;
        }
    }
    return false;
}

bool virtio_blk_full::read_sector(uint64_t sector, uint8_t* out_buf) {
    if (!blk_ready) return false;
    void* header = dma::alloc(sizeof(VirtioBlkReqFull), 4096);
    void* data = dma::alloc(512, 512);
    void* status = dma::alloc(1, 1);
    if (!header || !data || !status) return false;
    VirtioBlkReqFull h;
    memset(&h,0,sizeof(h));
    h.type = BLK_T_IN;
    h.ioprio = 0;
    h.sector = sector;
    memcpy(header, &h, sizeof(h));
    memset(status, 0, 1);
    bool ok = submit_block_request_chain(header, data, status);
    if (ok) memcpy(out_buf, data, 512);
    return ok;
}

bool virtio_blk_full::write_sector(uint64_t sector, const uint8_t* in_buf) {
    if (!blk_ready) return false;
    void* header = dma::alloc(sizeof(VirtioBlkReqFull), 4096);
    void* data = dma::alloc(512, 512);
    void* status = dma::alloc(1, 1);
    if (!header || !data || !status) return false;
    VirtioBlkReqFull h;
    memset(&h,0,sizeof(h));
    h.type = BLK_T_OUT;
    h.ioprio = 0;
    h.sector = sector;
    memcpy(header, &h, sizeof(h));
    memcpy(data, in_buf, 512);
    memset(status, 0, 1);
    bool ok = submit_block_request_chain(header, data, status);
    return ok;
}
