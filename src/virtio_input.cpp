#include "virtio_input.hpp"
#include "virtio_common.hpp"
#include "virtqueue.hpp"
#include "virtqueue_ops.hpp"
#include "dma.hpp"
#include <efi.h>
#include <efilib.h>
#include <string.h>

static bool inited = false;
static VirtqDesc* g_input_desc = nullptr;
static void* g_input_mem = nullptr;
static uint32_t g_input_qsize = 8;
static VirtQueueView g_input_vq;

// small FIFO for bytes extracted from input buffers
static uint8_t g_fifo[1024];
static unsigned g_fifo_head = 0;
static unsigned g_fifo_tail = 0;

static inline bool fifo_push(uint8_t b) {
    unsigned next = (g_fifo_tail + 1) % sizeof(g_fifo);
    if (next == g_fifo_head) return false; // full
    g_fifo[g_fifo_tail] = b;
    g_fifo_tail = next;
    return true;
}
static inline int fifo_pop() {
    if (g_fifo_head == g_fifo_tail) return -1;
    int v = g_fifo[g_fifo_head];
    g_fifo_head = (g_fifo_head + 1) % sizeof(g_fifo);
    return v;
}

// Helper to submit receive buffers to the virtqueue (legacy layout assumed)
static void populate_receive_buffers() {
    // For each descriptor index, allocate a small DMA buffer and submit it as write-only (device -> driver)
    for (uint32_t i = 0; i < g_input_vq.size; ++i) {
        // allocate 64 bytes per buffer
        void* buf = dma::alloc(64, 64);
        if (!buf) break;
        uint64_t addr = (uint64_t)(UINTN)buf;
        // flags: write-only (device writes into buffer). In legacy virtio, write flag is 2.
        const uint16_t VIRTQ_DESC_F_WRITE = 2;
        virtqueue_ops::set_descriptor(&g_input_vq, i, addr, 64, VIRTQ_DESC_F_WRITE, 0);
        virtqueue_ops::submit_descriptor(&g_input_vq, i);
    }
}

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

    g_input_vq = virtqueue_ops::view_from_mem(g_input_mem, g_input_qsize);
    virtqueue_ops::init_rings(&g_input_vq);

    // Try programming queue pfn (legacy) — best effort
    // Use existing helper; ignore return
    extern bool program_queue_pfn(const virtio_common::DeviceHandle* h, uint16_t queue_sel, void* pfn_mem);
    program_queue_pfn(&h, 0, g_input_mem);

    // populate receive buffers so device can write events
    populate_receive_buffers();

    inited = true;
    Print(L"virtio_input: initialized and receive buffers submitted (legacy best-effort)\n");
}

int8_t virtio_input::read_byte_nonblocking() {
    if (!inited) return INT8_MIN;
    // Poll used ring for completed buffers and push their contents into FIFO
    uint32_t id; uint32_t len;
    while (virtqueue_ops::try_dequeue_used(&g_input_vq, &id, &len)) {
        // id is descriptor index; retrieve buffer addr from desc
        uint64_t addr = g_input_vq.desc[id].addr;
        uint8_t* buf = (uint8_t*)(UINTN)addr;
        // push bytes into FIFO
        for (uint32_t i = 0; i < len; ++i) {
            if (!fifo_push(buf[i])) break;
        }
        // Re-submit buffer for future use
        const uint16_t VIRTQ_DESC_F_WRITE = 2;
        virtqueue_ops::set_descriptor(&g_input_vq, id, addr, 64, VIRTQ_DESC_F_WRITE, 0);
        virtqueue_ops::submit_descriptor(&g_input_vq, id);
    }

    int v = fifo_pop();
    if (v < 0) return INT8_MIN;
    return (int8_t)v;
}
