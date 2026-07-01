#include "virtio_blk.hpp"
#include "virtio_common.hpp"
#include "virtqueue.hpp"
#include "dma.hpp"
#include <efi.h>
#include <efilib.h>

static bool blk_ready = false;
static VirtqDesc* g_blk_desc = nullptr;
static void* g_blk_mem = nullptr;
static uint32_t g_blk_qsize = 8;

bool virtio_blk::init() {
    if (blk_ready) return true;
    virtio_common::DeviceHandle h;
    if (!virtio_common::probe_device(virtio_common::DeviceType::BLOCK, &h)) {
        Print(L"virtio-blk: no device found\n");
        return false;
    }
    if (!virtio_common::device_init(&h)) {
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

    // TODO: set up driver features, queue addresses, and start accepting requests. For now
    // we return success to indicate the device was found and resources allocated.
    blk_ready = true;
    Print(L"virtio-blk: initialized (virtqueue allocated)\n");
    return true;
}

bool virtio_blk::read_sector(uint64_t sector, uint8_t* out_buf) {
    (void)sector; (void)out_buf;
    // TODO: construct request, place buffers in descriptor table, notify device, wait for completion.
    return false;
}

bool virtio_blk::write_sector(uint64_t sector, const uint8_t* in_buf) {
    (void)sector; (void)in_buf;
    // TODO: implement block write via virtqueue.
    return false;
}
