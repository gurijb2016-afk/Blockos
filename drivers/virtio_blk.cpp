#include "virtio_blk.hpp"
#include "virtio_common.hpp"
#include "virtqueue.hpp"
#include "virtqueue_ops.hpp"
#include "dma.hpp"

#include <efi.h>
#include <efilib.h>
#include <string.h>
#include <stdint.h>

static bool blk_ready = false;

static VirtqDesc* g_blk_desc = nullptr;
static void* g_blk_mem = nullptr;

static uint32_t g_blk_qsize = 8;
static VirtQueueView g_blk_vq;

static virtio_common::DeviceHandle g_blk_handle;

#pragma pack(push, 1)
struct VirtioBlkReq {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
};
#pragma pack(pop)

enum {
    VIRTIO_BLK_T_IN  = 0,
    VIRTIO_BLK_T_OUT = 1
};

// VirtIO descriptor flags
static const uint16_t VIRTQ_DESC_F_NEXT  = 1;
static const uint16_t VIRTQ_DESC_F_WRITE = 2;

// --------------------------------------------------------------------------
// Legacy VirtIO PCI register offsets
// --------------------------------------------------------------------------

static const uint32_t VIRTIO_PCI_QUEUE_NOTIFY = 0x10;

// --------------------------------------------------------------------------
// Legacy queue PFN programming
// --------------------------------------------------------------------------

extern bool program_queue_pfn(
    const virtio_common::DeviceHandle* h,
    uint16_t queue_sel,
    void* pfn_mem
);

// --------------------------------------------------------------------------
// Initialize VirtIO block device
// --------------------------------------------------------------------------

bool virtio_blk::init()
{
    if (blk_ready) {
        return true;
    }

    //
    // Find VirtIO block device
    //
    if (!virtio_common::probe_device(
            virtio_common::DeviceType::BLOCK,
            &g_blk_handle))
    {
        Print((CHAR16*)L"virtio-blk: no device found\n");
        return false;
    }

    //
    // Basic VirtIO device initialization
    //
    if (!virtio_common::device_init(&g_blk_handle)) {
        Print((CHAR16*)L"virtio-blk: device_init failed\n");
        return false;
    }

    //
    // Calculate virtqueue memory size.
    //
    // Descriptor table:
    //   sizeof(VirtqDesc) * qsize
    //
    // Available ring:
    //   header + qsize * uint16_t
    //
    // Used ring:
    //   header + qsize * VirtqUsedElem
    //
    // Extra space is reserved for alignment.
    //

    size_t desc_size =
        sizeof(VirtqDesc) * g_blk_qsize;

    size_t avail_size =
        sizeof(VirtqAvail) +
        sizeof(uint16_t) * g_blk_qsize;

    size_t used_size =
        sizeof(VirtqUsed) +
        sizeof(VirtqUsedElem) * g_blk_qsize;

    size_t perq =
        desc_size +
        avail_size +
        used_size +
        1024;

    //
    // Allocate DMA-capable memory.
    //
    g_blk_mem = dma::alloc(perq, 4096);

    if (!g_blk_mem) {
        Print((CHAR16*)L"virtio-blk: dma alloc failed\n");
        return false;
    }

    //
    // Allocate / initialize virtqueue descriptor structures.
    //
    uint32_t desc_count = 0;

    g_blk_desc =
        virtqueue::alloc_virtqueue(
            g_blk_mem,
            g_blk_qsize,
            &desc_count
        );

    if (!g_blk_desc) {
        Print((CHAR16*)L"virtio-blk: virtqueue alloc failed\n");
        return false;
    }

    //
    // Create a view of the queue memory.
    //
    g_blk_vq =
        virtqueue_ops::view_from_mem(
            g_blk_mem,
            g_blk_qsize
        );

    //
    // Clear descriptor / available / used rings.
    //
    virtqueue_ops::init_rings(&g_blk_vq);

    //
    // Program queue 0 for legacy VirtIO.
    //
    if (!program_queue_pfn(
            &g_blk_handle,
            0,
            g_blk_mem))
    {
        Print(
            (CHAR16*)
            L"virtio-blk: program_queue_pfn failed "
            L"(device may need modern setup)\n"
        );

        //
        // Do not immediately fail here.
        //
        // Modern VirtIO devices may not use the legacy PFN mechanism.
        //
    }

    blk_ready = true;

    Print(
        (CHAR16*)
        L"virtio-blk: initialized "
        L"(virtqueue allocated)\n"
    );

    return true;
}

// --------------------------------------------------------------------------
// Notify VirtIO device
// --------------------------------------------------------------------------

static void notify_queue(uint16_t queue_index)
{
    if (g_blk_handle.mmio) {

        //
        // Legacy MMIO notification.
        //
        volatile uint16_t* notify =
            (volatile uint16_t*)
            (UINTN)(
                g_blk_handle.bar0 +
                VIRTIO_PCI_QUEUE_NOTIFY
            );

        *notify = queue_index;

    } else {

        //
        // Legacy PCI I/O notification.
        //
        uint16_t port =
            (uint16_t)(
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

// --------------------------------------------------------------------------
// Perform one synchronous VirtIO block request
// --------------------------------------------------------------------------

static bool perform_block_request(
    uint32_t type,
    uint64_t sector,
    uint8_t* data_buf,
    uint8_t* status_buf
)
{
    if (!blk_ready) {
        return false;
    }

    if (!data_buf || !status_buf) {
        return false;
    }

    //
    // We use three descriptors:
    //
    // descriptor 0 = VirtioBlkReq
    // descriptor 1 = sector data
    // descriptor 2 = status byte
    //

    //
    // Allocate request header in DMA memory.
    //
    void* header_dma =
        dma::alloc(
            sizeof(VirtioBlkReq),
            4096
        );

    if (!header_dma) {
        Print(
            (CHAR16*)
            L"virtio-blk: request header DMA allocation failed\n"
        );

        return false;
    }

    //
    // Construct request.
    //
    VirtioBlkReq req;

    memset(
        &req,
        0,
        sizeof(req)
    );

    req.type = type;
    req.ioprio = 0;
    req.sector = sector;

    memcpy(
        header_dma,
        &req,
        sizeof(req)
    );

    //
    // Descriptor 0:
    //
    // Device reads the request header.
    //
    virtqueue_ops::set_descriptor(
        &g_blk_vq,
        0,
        (uint64_t)(UINTN)header_dma,
        sizeof(VirtioBlkReq),
        VIRTQ_DESC_F_NEXT,
        1
    );

    //
    // Descriptor 1:
    //
    // IN:
    //   device writes data to memory
    //
    // OUT:
    //   device reads data from memory
    //
    uint16_t data_flags = 0;

    if (type == VIRTIO_BLK_T_IN) {
        data_flags = VIRTQ_DESC_F_WRITE;
    }

    virtqueue_ops::set_descriptor(
        &g_blk_vq,
        1,
        (uint64_t)(UINTN)data_buf,
        512,
        data_flags | VIRTQ_DESC_F_NEXT,
        2
    );

    //
    // Descriptor 2:
    //
    // Device writes the status byte.
    //
    virtqueue_ops::set_descriptor(
        &g_blk_vq,
        2,
        (uint64_t)(UINTN)status_buf,
        1,
        VIRTQ_DESC_F_WRITE,
        0
    );

    //
    // Put descriptor chain into available ring.
    //
    virtqueue_ops::submit_descriptor(
        &g_blk_vq,
        0
    );

    //
    // Notify queue 0.
    //
    notify_queue(0);

    //
    // Poll used ring.
    //
    uint32_t id = 0;
    uint32_t len = 0;

    for (uint32_t i = 0; i < 1000000; ++i) {

        if (virtqueue_ops::try_dequeue_used(
                &g_blk_vq,
                &id,
                &len))
        {
            //
            // VirtIO block status:
            //
            // 0 = VIRTIO_BLK_S_OK
            // 1 = VIRTIO_BLK_S_IOERR
            // 2 = VIRTIO_BLK_S_UNSUPP
            //

            uint8_t status =
                *(volatile uint8_t*)
                (UINTN)status_buf;

            //
            // Descriptor chain should start at 0.
            //
            if (id != 0) {
                Print(
                    (CHAR16*)
                    L"virtio-blk: unexpected used descriptor id=%u\n",
                    id
                );
            }

            if (status == 0) {
                return true;
            }

            Print(
                (CHAR16*)
                L"virtio-blk: request failed, status=%u\n",
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

// --------------------------------------------------------------------------
// Read one 512-byte sector
// --------------------------------------------------------------------------

bool virtio_blk::read_sector(
    uint64_t sector,
    uint8_t* out_buf
)
{
    if (!blk_ready) {
        return false;
    }

    if (!out_buf) {
        return false;
    }

    //
    // DMA data buffer.
    //
    void* data_dma =
        dma::alloc(
            512,
            512
        );

    if (!data_dma) {
        Print(
            (CHAR16*)
            L"virtio-blk: read data DMA allocation failed\n"
        );

        return false;
    }

    //
    // DMA status byte.
    //
    void* status_dma =
        dma::alloc(
            1,
            1
        );

    if (!status_dma) {
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

    //
    // Execute READ request.
    //
    bool ok =
        perform_block_request(
            VIRTIO_BLK_T_IN,
            sector,
            (uint8_t*)data_dma,
            (uint8_t*)status_dma
        );

    //
    // Copy data to caller buffer.
    //
    if (ok) {
        memcpy(
            out_buf,
            data_dma,
            512
        );
    }

    return ok;
}

// --------------------------------------------------------------------------
// Write one 512-byte sector
// --------------------------------------------------------------------------

bool virtio_blk::write_sector(
    uint64_t sector,
    const uint8_t* in_buf
)
{
    if (!blk_ready) {
        return false;
    }

    if (!in_buf) {
        return false;
    }

    //
    // DMA data buffer.
    //
    void* data_dma =
        dma::alloc(
            512,
            512
        );

    if (!data_dma) {
        Print(
            (CHAR16*)
            L"virtio-blk: write data DMA allocation failed\n"
        );

        return false;
    }

    //
    // DMA status byte.
    //
    void* status_dma =
        dma::alloc(
            1,
            1
        );

    if (!status_dma) {
        Print(
            (CHAR16*)
            L"virtio-blk: write status DMA allocation failed\n"
        );

        return false;
    }

    //
    // Copy caller data into DMA buffer.
    //
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

    //
    // Execute WRITE request.
    //
    bool ok =
        perform_block_request(
            VIRTIO_BLK_T_OUT,
            sector,
            (uint8_t*)data_dma,
            (uint8_t*)status_dma
        );

    return ok;
}
