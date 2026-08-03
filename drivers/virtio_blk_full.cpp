#include "virtio_blk_full.hpp"
#include "virtio_common.hpp"
#include "virtqueue.hpp"
#include "virtqueue_ops.hpp"
#include "virtio_notify.hpp"
#include "dma.hpp"

#include <efi.h>
#include <efilib.h>
#include <string.h>
#include <stdint.h>

// -----------------------------------------------------------------------------
// Shared state from virtio_blk.cpp
// -----------------------------------------------------------------------------

extern bool virtio_blk_is_ready();

// -----------------------------------------------------------------------------
// VirtIO block request
// -----------------------------------------------------------------------------

#pragma pack(push, 1)
struct VirtioBlkReqFull {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
};
#pragma pack(pop)

// VirtIO block request types
enum {
    BLK_T_IN  = 0,
    BLK_T_OUT = 1
};

// -----------------------------------------------------------------------------
// Generic request helper
// -----------------------------------------------------------------------------
//
// Descriptor chain:
//
//   0 -> request header
//   1 -> data buffer
//   2 -> status byte
//
// For reads, the data descriptor is VIRTQ_DESC_F_WRITE.
// For writes, it is a normal descriptor.
//
// This function uses the existing virtio_blk infrastructure.
// -----------------------------------------------------------------------------

static bool submit_block_request_chain_generic(
    void* header,
    void* data,
    uint32_t data_len,
    void* status
) {
    if (!header || !data || !status || data_len == 0)
        return false;

    /*
     * The actual request submission is intentionally kept in
     * virtio_blk.cpp.
     *
     * The full driver should use the same initialized queue instead
     * of creating a second independent VirtQueueView.
     */
    extern bool virtio_blk_submit_request(
        void* header,
        void* data,
        uint32_t data_len,
        void* status,
        bool device_writes_data
    );

    return virtio_blk_submit_request(
        header,
        data,
        data_len,
        status,
        false
    );
}

// -----------------------------------------------------------------------------
// Read multiple sectors
// -----------------------------------------------------------------------------

bool virtio_blk_full::read_sectors(
    uint64_t sector,
    uint32_t count,
    uint8_t* out_buf
) {
    if (!virtio_blk_is_ready())
        return false;

    if (count == 0 || !out_buf)
        return false;

    uint64_t total = (uint64_t)count * 512ULL;

    // -------------------------------------------------------------------------
    // Fast path: one contiguous request
    // -------------------------------------------------------------------------

    if (total <= 1024ULL * 1024ULL) {
        void* header = dma::alloc(
            sizeof(VirtioBlkReqFull),
            4096
        );

        void* data = dma::alloc(
            (size_t)total,
            512
        );

        void* status = dma::alloc(
            1,
            1
        );

        if (header && data && status) {
            VirtioBlkReqFull h;

            memset(&h, 0, sizeof(h));

            h.type   = BLK_T_IN;
            h.ioprio = 0;
            h.sector = sector;

            memcpy(
                header,
                &h,
                sizeof(h)
            );

            memset(
                status,
                0,
                1
            );

            extern bool virtio_blk_submit_request(
                void* header,
                void* data,
                uint32_t data_len,
                void* status,
                bool device_writes_data
            );

            bool ok = virtio_blk_submit_request(
                header,
                data,
                (uint32_t)total,
                status,
                true
            );

            if (ok) {
                memcpy(
                    out_buf,
                    data,
                    (size_t)total
                );
            }

            return ok;
        }
    }

    // -------------------------------------------------------------------------
    // Fallback: one sector at a time
    // -------------------------------------------------------------------------

    for (uint32_t i = 0; i < count; ++i) {
        void* header = dma::alloc(
            sizeof(VirtioBlkReqFull),
            4096
        );

        void* data = dma::alloc(
            512,
            512
        );

        void* status = dma::alloc(
            1,
            1
        );

        if (!header || !data || !status)
            return false;

        VirtioBlkReqFull h;

        memset(
            &h,
            0,
            sizeof(h)
        );

        h.type   = BLK_T_IN;
        h.ioprio = 0;
        h.sector = sector + i;

        memcpy(
            header,
            &h,
            sizeof(h)
        );

        memset(
            status,
            0,
            1
        );

        extern bool virtio_blk_submit_request(
            void* header,
            void* data,
            uint32_t data_len,
            void* status,
            bool device_writes_data
        );

        bool ok = virtio_blk_submit_request(
            header,
            data,
            512,
            status,
            true
        );

        if (!ok)
            return false;

        memcpy(
            out_buf + ((size_t)i * 512),
            data,
            512
        );
    }

    return true;
}

// -----------------------------------------------------------------------------
// Write multiple sectors
// -----------------------------------------------------------------------------

bool virtio_blk_full::write_sectors(
    uint64_t sector,
    uint32_t count,
    const uint8_t* in_buf
) {
    if (!virtio_blk_is_ready())
        return false;

    if (count == 0 || !in_buf)
        return false;

    uint64_t total = (uint64_t)count * 512ULL;

    // -------------------------------------------------------------------------
    // Fast path: one contiguous request
    // -------------------------------------------------------------------------

    if (total <= 1024ULL * 1024ULL) {
        void* header = dma::alloc(
            sizeof(VirtioBlkReqFull),
            4096
        );

        void* data = dma::alloc(
            (size_t)total,
            512
        );

        void* status = dma::alloc(
            1,
            1
        );

        if (header && data && status) {
            memcpy(
                data,
                in_buf,
                (size_t)total
            );

            VirtioBlkReqFull h;

            memset(
                &h,
                0,
                sizeof(h)
            );

            h.type   = BLK_T_OUT;
            h.ioprio = 0;
            h.sector = sector;

            memcpy(
                header,
                &h,
                sizeof(h)
            );

            memset(
                status,
                0,
                1
            );

            extern bool virtio_blk_submit_request(
                void* header,
                void* data,
                uint32_t data_len,
                void* status,
                bool device_writes_data
            );

            return virtio_blk_submit_request(
                header,
                data,
                (uint32_t)total,
                status,
                false
            );
        }
    }

    // -------------------------------------------------------------------------
    // Fallback: one sector at a time
    // -------------------------------------------------------------------------

    for (uint32_t i = 0; i < count; ++i) {
        void* header = dma::alloc(
            sizeof(VirtioBlkReqFull),
            4096
        );

        void* data = dma::alloc(
            512,
            512
        );

        void* status = dma::alloc(
            1,
            1
        );

        if (!header || !data || !status)
            return false;

        memcpy(
            data,
            in_buf + ((size_t)i * 512),
            512
        );

        VirtioBlkReqFull h;

        memset(
            &h,
            0,
            sizeof(h)
        );

        h.type   = BLK_T_OUT;
        h.ioprio = 0;
        h.sector = sector + i;

        memcpy(
            header,
            &h,
            sizeof(h)
        );

        memset(
            status,
            0,
            1
        );

        extern bool virtio_blk_submit_request(
            void* header,
            void* data,
            uint32_t data_len,
            void* status,
            bool device_writes_data
        );

        bool ok = virtio_blk_submit_request(
            header,
            data,
            512,
            status,
            false
        );

        if (!ok)
            return false;
    }

    return true;
}
