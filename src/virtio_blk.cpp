#include "virtio_blk.hpp"
#include "virtio_common.hpp"
#include "virtqueue.hpp"
#include "virtqueue_ops.hpp"
#include "dma.hpp"
#include <efi.h>
#include <efilib.h>
#include <string.h>

static bool blk_ready = false;
static VirtqDesc* g_blk_desc = nullptr;
static void* g_blk_mem = nullptr;
static uint32_t g_blk_qsize = 8;
static VirtQueueView g_blk_vq;
static virtio_common::DeviceHandle g_blk_handle;

#pragma pack(push,1)
struct VirtioBlkReq {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
};
#pragma pack(pop)

enum {
    VIRTIO_BLK_T_IN = 0,
    VIRTIO_BLK_T_OUT = 1
};

bool virtio_blk::init() {
    if (blk_ready) return true;
    if (!virtio_common::probe_device(virtio_common::DeviceType::BLOCK, &g_blk_handle)) {
        Print(L"virtio-blk: no device found\n");
        return false;
    }
    if (!virtio_common::device_init(&g_blk_handle)) {
        Print(L"virtio-blk: device_init failed\n");
        return false;
    }

    size_t perq = (sizeof(VirtqDesc) * g_blk_qsize) + (sizeof(uint16_t) * g_blk_qsize) + (sizeof(VirtqUsedElem) * g_blk_qsize) + 1024;
    g_blk_mem = dma::alloc(perq, 4096);
    if (!g_blk_mem) {
        Print(L"virtio-blk: dma alloc failed\n");
        return false;
    }
    uint32_t desc_count = 0;
    g_blk_desc = virtqueue::alloc_virtqueue(g_blk_mem, g_blk_qsize, &desc_count);
    if (!g_blk_desc) {
        Print(L"virtio-blk: virtqueue alloc failed\n");
        return false;
    }

    g_blk_vq = virtqueue_ops::view_from_mem(g_blk_mem, g_blk_qsize);
    virtqueue_ops::init_rings(&g_blk_vq);

    // Attempt legacy queue PFN programming (best-effort)
    extern bool program_queue_pfn(const virtio_common::DeviceHandle* h, uint16_t queue_sel, void* pfn_mem);
    if (!program_queue_pfn(&g_blk_handle, 0, g_blk_mem)) {
        Print(L"virtio-blk: program_queue_pfn failed (device may need modern setup)\n");
    }

    blk_ready = true;
    Print(L"virtio-blk: initialized (virtqueue allocated)\n");
    return true;
}

// Internal helper: perform a synchronous block request (type IN/OUT). Returns true on success.
static bool perform_block_request(uint32_t type, uint64_t sector, uint8_t* data_buf, uint8_t* status_buf) {
    if (!blk_ready) return false;
    // Use descriptor indices 0=header,1=data,2=status
    const uint16_t VIRTQ_DESC_F_WRITE = 2;
    // allocate header in DMA memory (we could reuse, but keep simple)
    void* header_dma = dma::alloc(sizeof(VirtioBlkReq), 4096);
    if (!header_dma) return false;
    VirtioBlkReq hr;
    memset(&hr, 0, sizeof(hr));
    hr.type = type;
    hr.ioprio = 0;
    hr.sector = sector;
    memcpy(header_dma, &hr, sizeof(hr));

    // data_buf must be DMA-allocated by caller (we assume so)
    // status_buf must be DMA-allocated by caller

    // set descriptor 0: header (device reads)
    virtqueue_ops::set_descriptor(&g_blk_vq, 0, (uint64_t)(UINTN)header_dma, sizeof(VirtioBlkReq), 0, 1);
    // descriptor 1: data (device writes for IN, device reads for OUT)
    uint16_t flags = (type == VIRTIO_BLK_T_IN) ? VIRTQ_DESC_F_WRITE : 0;
    virtqueue_ops::set_descriptor(&g_blk_vq, 1, (uint64_t)(UINTN)data_buf, 512, flags, 2);
    // descriptor 2: status (device writes)
    virtqueue_ops::set_descriptor(&g_blk_vq, 2, (uint64_t)(UINTN)status_buf, 1, VIRTQ_DESC_F_WRITE, 0);

    // submit starting at descriptor 0
    virtqueue_ops::submit_descriptor(&g_blk_vq, 0);

    // TODO: notify device (legacy QUEUE_NOTIFY or modern notify via MSI-X). Best-effort: write to queue notify if possible
    const uint32_t QUEUE_NOTIFY = 0x10; // best-effort guess — may be incorrect on some devices
    if (g_blk_handle.mmio) {
        volatile uint16_t* notify = (volatile uint16_t*)(UINTN)(g_blk_handle.bar0 + QUEUE_NOTIFY);
        *notify = 0;
    } else {
        uint16_t port = (uint16_t)(g_blk_handle.bar0 + QUEUE_NOTIFY);
        uint16_t v = 0;
        __asm__ volatile ("outw %0, %1" : : "a" (v), "dN" (port));
    }

    // wait for completion by polling used ring
    uint32_t id; uint32_t len;
    for (uint32_t i = 0; i < 1000000; ++i) {
        if (virtqueue_ops::try_dequeue_used(&g_blk_vq, &id, &len)) {
            // id should be 0 (descriptor index)
            // read status
            uint8_t st = *(uint8_t*)(UINTN)status_buf;
            return st == 0;
        }
    }

    return false;
}

bool virtio_blk::read_sector(uint64_t sector, uint8_t* out_buf) {
    if (!blk_ready) return false;
    // allocate DMA buffers for data and status
    void* data_dma = dma::alloc(512, 512);
    if (!data_dma) return false;
    void* status_dma = dma::alloc(1, 1);
    if (!status_dma) return false;
    // Ensure zeroed
    memset(data_dma, 0, 512);
    memset(status_dma, 0, 1);

    bool ok = perform_block_request(VIRTIO_BLK_T_IN, sector, (uint8_t*)data_dma, (uint8_t*)status_dma);
    if (ok) {
        memcpy(out_buf, data_dma, 512);
    }
    return ok;
}

bool virtio_blk::write_sector(uint64_t sector, const uint8_t* in_buf) {
    if (!blk_ready) return false;
    void* data_dma = dma::alloc(512, 512);
    if (!data_dma) return false;
    void* status_dma = dma::alloc(1, 1);
    if (!status_dma) return false;
    memcpy(data_dma, in_buf, 512);
    memset(status_dma, 0, 1);

    bool ok = perform_block_request(VIRTIO_BLK_T_OUT, sector, (uint8_t*)data_dma, (uint8_t*)status_dma);
    return ok;
}
