#include "virtio_blk_full.hpp"
#include "virtio_common.hpp"
#include "virtqueue.hpp"
#include "virtqueue_ops.hpp"
#include "virtio_notify.hpp"
#include "dma.hpp"
#include <efi.h>
#include <efilib.h>
#include <string.h>

// Updated read_sectors/write_sectors to attempt single-request batch when possible.
bool virtio_blk_full::read_sectors(uint64_t sector, uint32_t count, uint8_t* out_buf) {
    if (!blk_ready) return false;
    if (count == 0) return false;

    // Try to allocate a single contiguous data buffer for the whole range
    uint64_t total = (uint64_t)count * 512ULL;
    if (total > 0 && total < (1024 * 1024)) {
        void* header = dma::alloc(sizeof(VirtioBlkReqFull), 4096);
        void* data = dma::alloc((size_t)total, 512);
        void* status = dma::alloc(1, 1);
        if (header && data && status) {
            VirtioBlkReqFull h;
            memset(&h,0,sizeof(h));
            h.type = BLK_T_IN;
            h.ioprio = 0;
            h.sector = sector;
            memcpy(header, &h, sizeof(h));
            memset(status, 0, 1);
            bool ok = submit_block_request_chain_generic(header, data, (uint32_t)total, status);
            if (ok) memcpy(out_buf, data, (size_t)total);
            return ok;
        }
    }

    // Fallback to per-sector sequence
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

    uint64_t total = (uint64_t)count * 512ULL;
    if (total > 0 && total < (1024 * 1024)) {
        void* header = dma::alloc(sizeof(VirtioBlkReqFull), 4096);
        void* data = dma::alloc((size_t)total, 512);
        void* status = dma::alloc(1, 1);
        if (header && data && status) {
            memcpy(data, in_buf, (size_t)total);
            VirtioBlkReqFull h;
            memset(&h,0,sizeof(h));
            h.type = BLK_T_OUT;
            h.ioprio = 0;
            h.sector = sector;
            memcpy(header, &h, sizeof(h));
            memset(status, 0, 1);
            bool ok = submit_block_request_chain_generic(header, data, (uint32_t)total, status);
            return ok;
        }
    }

    // Fallback: per-sector write
    for (uint32_t i = 0; i < count; ++i) {
        void* header = dma::alloc(sizeof(VirtioBlkReqFull), 4096);
        void* data = dma::alloc(512, 512);
        void* status = dma::alloc(1, 1);
        if (!header || !data || !status) return false;
        memcpy(data, in_buf + i*512, 512);
        VirtioBlkReqFull h;
        memset(&h,0,sizeof(h));
        h.type = BLK_T_OUT;
        h.ioprio = 0;
        h.sector = sector + i;
        memcpy(header, &h, sizeof(h));
        memset(status, 0, 1);
        bool ok = submit_block_request_chain_generic(header, data, 512, status);
        if (!ok) return false;
    }
    return true;
}
