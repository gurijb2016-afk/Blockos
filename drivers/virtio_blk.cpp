#include "virtio_blk.hpp"
#include "virtio_common.hpp"
#include "virtqueue.hpp"
#include "virtqueue_ops.hpp"
#include "dma.hpp"

extern "C" {
#include <efi.h>
}

extern "C" {
#include <efilib.h>
}

#include <string.h>
#include <stdint.h>
#include <stddef.h>

namespace
{
    static bool blk_ready = false;

    static VirtqDesc* g_blk_desc = nullptr;
    static void* g_blk_mem = nullptr;

    static uint32_t g_blk_qsize = 8;
    static VirtQueueView g_blk_vq;

    static virtio_common::DeviceHandle g_blk_handle{};

#pragma pack(push, 1)
    struct VirtioBlkReq
    {
        uint32_t type;
        uint32_t ioprio;
        uint64_t sector;
    };
#pragma pack(pop)

    enum : uint32_t
    {
        VIRTIO_BLK_T_IN  = 0,
        VIRTIO_BLK_T_OUT = 1
    };

    enum : uint16_t
    {
        VIRTQ_DESC_F_NEXT  = 1,
        VIRTQ_DESC_F_WRITE = 2
    };

    static constexpr uint32_t VIRTIO_PCI_QUEUE_NOTIFY = 0x10;

    /*
     * Legacy VirtIO queue PFN programming.
     * Implemented by the VirtIO PCI/queue support code.
     */
    extern bool program_queue_pfn(
        const virtio_common::DeviceHandle* h,
        uint16_t queue_sel,
        void* pfn_mem
    );

    // --------------------------------------------------------
    // Queue notification
    // --------------------------------------------------------

    static void notify_queue(uint16_t queue_index)
    {
        if (g_blk_handle.mmio)
        {
            volatile uint16_t* notify =
                reinterpret_cast<volatile uint16_t*>(
                    static_cast<UINTN>(
                        g_blk_handle.bar0 +
                        VIRTIO_PCI_QUEUE_NOTIFY
                    )
                );

            *notify = queue_index;
        }
        else
        {
            uint16_t port =
                static_cast<uint16_t>(
                    g_blk_handle.bar0 +
                    VIRTIO_PCI_QUEUE_NOTIFY
                );

            uint16_t value = queue_index;

            __asm__ volatile (
                "outw %0, %1"
                :
                : "a"(value), "dN"(port)
            );
        }
    }

    // --------------------------------------------------------
    // One synchronous block request
    // --------------------------------------------------------

    static bool perform_block_request(
        uint32_t type,
        uint64_t sector,
        uint8_t* data_buf,
        uint8_t* status_buf
    )
    {
        if (!blk_ready)
        {
            return false;
        }

        if (data_buf == nullptr || status_buf == nullptr)
        {
            return false;
        }

        /*
         * Descriptor chain:
         *
         *   0 -> VirtioBlkReq
         *   1 -> sector data
         *   2 -> status
         */

        void* header_dma =
            dma::alloc(
                sizeof(VirtioBlkReq),
                4096
            );

        if (header_dma == nullptr)
        {
            Print(
                (CHAR16*)
                L"virtio-blk: request header DMA allocation failed\n"
            );

            return false;
        }

        VirtioBlkReq req{};

        req.type   = type;
        req.ioprio = 0;
        req.sector = sector;

        memcpy(
            header_dma,
            &req,
            sizeof(req)
        );

        /*
         * Descriptor 0:
         * device reads request header
         */
        virtqueue_ops::set_descriptor(
            &g_blk_vq,
            0,
            static_cast<uint64_t>(
                reinterpret_cast<UINTN>(header_dma)
            ),
            static_cast<uint32_t>(
                sizeof(VirtioBlkReq)
            ),
            VIRTQ_DESC_F_NEXT,
            1
        );

        /*
         * Descriptor 1:
         *
         * READ:
         *   device writes sector data
         *
         * WRITE:
         *   device reads sector data
         */
        uint16_t data_flags = 0;

        if (type == VIRTIO_BLK_T_IN)
        {
            data_flags = VIRTQ_DESC_F_WRITE;
        }

        virtqueue_ops::set_descriptor(
            &g_blk_vq,
            1,
            static_cast<uint64_t>(
                reinterpret_cast<UINTN>(data_buf)
            ),
            512,
            static_cast<uint16_t>(
                data_flags |
                VIRTQ_DESC_F_NEXT
            ),
            2
        );

        /*
         * Descriptor 2:
         * device writes status byte
         */
        virtqueue_ops::set_descriptor(
            &g_blk_vq,
            2,
            static_cast<uint64_t>(
                reinterpret_cast<UINTN>(status_buf)
            ),
            1,
            VIRTQ_DESC_F_WRITE,
            0
        );

        /*
         * Submit descriptor chain.
         */
        virtqueue_ops::submit_descriptor(
            &g_blk_vq,
            0
        );

        /*
         * Notify queue 0.
         */
        notify_queue(0);

        /*
         * Wait synchronously for used ring completion.
         */
        uint32_t used_id = 0;
        uint32_t used_len = 0;

        constexpr uint32_t TIMEOUT_LOOPS = 1000000;

        for (uint32_t i = 0;
             i < TIMEOUT_LOOPS;
             ++i)
        {
            if (!virtqueue_ops::try_dequeue_used(
                    &g_blk_vq,
                    &used_id,
                    &used_len))
            {
                continue;
            }

            uint8_t status =
                *reinterpret_cast<volatile uint8_t*>(
                    status_buf
                );

            if (used_id != 0)
            {
                Print(
                    (CHAR16*)
                    L"virtio-blk: unexpected used id=%u\n",
                    used_id
                );
            }

            switch (status)
            {
                case 0:
                    return true;

                case 1:
                    Print(
                        (CHAR16*)
                        L"virtio-blk: I/O error\n"
                    );
                    return false;

                case 2:
                    Print(
                        (CHAR16*)
                        L"virtio-blk: unsupported request\n"
                    );
                    return false;

                default:
                    Print(
                        (CHAR16*)
                        L"virtio-blk: unknown status=%u\n",
                        status
                    );
                    return false;
            }
        }

        Print(
            (CHAR16*)
            L"virtio-blk: request timeout\n"
        );

        return false;
    }
}

// ============================================================
// VirtIO block initialization
// ============================================================

bool virtio_blk::init()
{
    if (blk_ready)
    {
        return true;
    }

    /*
     * Find VirtIO block device.
     */
    if (!virtio_common::probe_device(
            virtio_common::DeviceType::BLOCK,
            &g_blk_handle))
    {
        Print(
            (CHAR16*)
            L"virtio-blk: no device found\n"
        );

        return false;
    }

    /*
     * Initialize device.
     */
    if (!virtio_common::device_init(
            &g_blk_handle))
    {
        Print(
            (CHAR16*)
            L"virtio-blk: device_init failed\n"
        );

        return false;
    }

    /*
     * Calculate virtqueue memory.
     */

    const size_t desc_size =
        sizeof(VirtqDesc) *
        g_blk_qsize;

    const size_t avail_size =
        sizeof(VirtqAvail) +
        sizeof(uint16_t) *
        g_blk_qsize;

    const size_t used_size =
        sizeof(VirtqUsed) +
        sizeof(VirtqUsedElem) *
        g_blk_qsize;

    const size_t queue_mem_size =
        desc_size +
        avail_size +
        used_size +
        1024;

    /*
     * Allocate queue memory.
     */
    g_blk_mem =
        dma::alloc(
            queue_mem_size,
            4096
        );

    if (g_blk_mem == nullptr)
    {
        Print(
            (CHAR16*)
            L"virtio-blk: DMA allocation failed\n"
        );

        return false;
    }

    /*
     * Allocate/init queue.
     */
    uint32_t desc_count = 0;

    g_blk_desc =
        virtqueue::alloc_virtqueue(
            g_blk_mem,
            g_blk_qsize,
            &desc_count
        );

    if (g_blk_desc == nullptr)
    {
        Print(
            (CHAR16*)
            L"virtio-blk: virtqueue allocation failed\n"
        );

        return false;
    }

    if (desc_count < 3)
    {
        Print(
            (CHAR16*)
            L"virtio-blk: queue has insufficient descriptors\n"
        );

        return false;
    }

    /*
     * Build queue view.
     */
    g_blk_vq =
        virtqueue_ops::view_from_mem(
            g_blk_mem,
            g_blk_qsize
        );

    /*
     * Initialize queue rings.
     */
    virtqueue_ops::init_rings(
        &g_blk_vq
    );

    /*
     * Legacy queue PFN setup.
     */
    if (!program_queue_pfn(
            &g_blk_handle,
            0,
            g_blk_mem))
    {
        Print(
            (CHAR16*)
            L"virtio-blk: program_queue_pfn failed "
            L"(modern device may require modern setup)\n"
        );
    }

    blk_ready = true;

    Print(
        (CHAR16*)
        L"virtio-blk: initialized "
        L"(queue=%u)\n",
        g_blk_qsize
    );

    return true;
}

// ============================================================
// Read one 512-byte sector
// ============================================================

bool virtio_blk::read_sector(
    uint64_t sector,
    uint8_t* out_buf
)
{
    if (!blk_ready)
    {
        return false;
    }

    if (out_buf == nullptr)
    {
        return false;
    }

    /*
     * Allocate DMA data buffer.
     */
    void* data_dma =
        dma::alloc(
            512,
            512
        );

    if (data_dma == nullptr)
    {
        Print(
            (CHAR16*)
            L"virtio-blk: read data DMA allocation failed\n"
        );

        return false;
    }

    /*
     * Allocate DMA status byte.
     */
    void* status_dma =
        dma::alloc(
            1,
            1
        );

    if (status_dma == nullptr)
    {
        Print(
            (CHAR16*)
            L"virtio-blk: read status DMA allocation failed\n"
        );

        return false;
    }

    memset(
        data_dma,
        0,
        512
    );

    memset(
        status_dma,
        0,
        1
    );

    /*
     * Perform READ.
     */
    const bool ok =
        perform_block_request(
            VIRTIO_BLK_T_IN,
            sector,
            reinterpret_cast<uint8_t*>(
                data_dma
            ),
            reinterpret_cast<uint8_t*>(
                status_dma
            )
        );

    /*
     * Copy result back.
     */
    if (ok)
    {
        memcpy(
            out_buf,
            data_dma,
            512
        );
    }

    return ok;
}

// ============================================================
// Write one 512-byte sector
// ============================================================

bool virtio_blk::write_sector(
    uint64_t sector,
    const uint8_t* in_buf
)
{
    if (!blk_ready)
    {
        return false;
    }

    if (in_buf == nullptr)
    {
        return false;
    }

    /*
     * Allocate DMA data buffer.
     */
    void* data_dma =
        dma::alloc(
            512,
            512
        );

    if (data_dma == nullptr)
    {
        Print(
            (CHAR16*)
            L"virtio-blk: write data DMA allocation failed\n"
        );

        return false;
    }

    /*
     * Allocate DMA status byte.
     */
    void* status_dma =
        dma::alloc(
            1,
            1
        );

    if (status_dma == nullptr)
    {
        Print(
            (CHAR16*)
            L"virtio-blk: write status DMA allocation failed\n"
        );

        return false;
    }

    /*
     * Copy source data into DMA buffer.
     */
    memcpy(
        data_dma,
        in_buf,
        512
    );

    memset(
        status_dma,
        0,
        1
    );

    /*
     * Perform WRITE.
     */
    return perform_block_request(
        VIRTIO_BLK_T_OUT,
        sector,
        reinterpret_cast<uint8_t*>(
            data_dma
        ),
        reinterpret_cast<uint8_t*>(
            status_dma
        )
    );
}
