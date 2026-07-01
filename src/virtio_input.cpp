#include "virtio_input.hpp"
#include "virtio_common.hpp"
#include "virtqueue.hpp"
#include "dma.hpp"
#include "events.hpp"
#include <efi.h>
#include <efilib.h>
#include <string.h>

static bool inited = false;
static VirtqDesc* g_input_desc = nullptr;
static void* g_input_mem = nullptr;
static uint32_t g_input_qsize = 8;

void virtio_input::init() {
    if (inited) return;
    virtio_common::DeviceHandle h;
    if (!virtio_common::probe_device(virtio_common::DeviceType::INPUT, &h)) {
        Print(L"virtio_input: no device found, falling back to PS/2\n");
        inited = false;
        return;
    }
    if (!virtio_common::device_init(&h)) {
        Print(L"virtio_input: device_init failed\n");
        inited = false;
        return;
    }

    size_t perq = (sizeof(VirtqDesc) * g_input_qsize) + (sizeof(uint16_t) * g_input_qsize) + (sizeof(VirtqUsedElem) * g_input_qsize) + 1024;
    g_input_mem = dma::alloc(perq, 4096);
    if (!g_input_mem) {
        Print(L"virtio_input: dma alloc for virtqueue failed\n");
        inited = false;
        return;
    }

    uint32_t desc_count = 0;
    g_input_desc = virtqueue::alloc_virtqueue(g_input_mem, g_input_qsize, &desc_count);
    if (!g_input_desc) {
        Print(L"virtio_input: virtqueue alloc failed\n");
        inited = false;
        return;
    }

    // TODO: program device with queue addresses and notify. For now we mark initialized so
    // future code can check availability and optionally use PS/2 fallback.
    Print(L"virtio_input: initialized (virtqueue allocated)\n");
    inited = true;
}

int8_t virtio_input::read_byte_nonblocking() {
    // TODO: poll virtqueue for event descriptors and convert them to bytes or Event pushes.
    // For now, not implemented; rely on PS/2 fallback in kernel loop.
    return INT8_MIN;
}
