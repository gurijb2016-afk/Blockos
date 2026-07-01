#include "virtio_net_driver.hpp"
#include "virtio_common.hpp"
#include "virtqueue.hpp"
#include "virtqueue_ops.hpp"
#include "dma.hpp"
#include <efi.h>
#include <efilib.h>
#include <string.h>

static bool g_ready = false;
static VirtqDesc* g_rx_desc = nullptr;
static VirtqDesc* g_tx_desc = nullptr;
static void* g_rx_mem = nullptr;
static void* g_tx_mem = nullptr;
static uint32_t g_qsize = 8;
static VirtQueueView g_rx_vq;
static VirtQueueView g_tx_vq;
static virtio_common::DeviceHandle g_net_handle;

// Simple TX buffer pool
static void* g_tx_pool[32];
static int g_tx_pool_head = 0;
static int g_tx_pool_count = 0;

static inline void tx_pool_push(void* p) {
    if (g_tx_pool_count >= (int)(sizeof(g_tx_pool)/sizeof(g_tx_pool[0]))) return;
    g_tx_pool[g_tx_pool_head] = p;
    g_tx_pool_head = (g_tx_pool_head + 1) % (int)(sizeof(g_tx_pool)/sizeof(g_tx_pool[0]));
    g_tx_pool_count++;
}
static inline void* tx_pool_pop() {
    if (g_tx_pool_count == 0) return NULL;
    int tail = (g_tx_pool_head - g_tx_pool_count) % (int)(sizeof(g_tx_pool)/sizeof(g_tx_pool[0]));
    if (tail < 0) tail += (int)(sizeof(g_tx_pool)/sizeof(g_tx_pool[0]));
    void* p = g_tx_pool[tail];
    g_tx_pool[tail] = NULL;
    g_tx_pool_count--;
    return p;
}

// Local helper to program legacy queue PFN (best-effort)
static bool program_queue_pfn_local(const virtio_common::DeviceHandle* h, uint16_t queue_sel, void* pfn_mem) {
    const uint32_t QUEUE_SELECT = 0x0C; // 16-bit
    const uint32_t QUEUE_NUM = 0x0E; // 16-bit
    const uint32_t QUEUE_PFN = 0x10; // 32-bit
    uint16_t qsel = queue_sel;
    if (h->mmio) {
        volatile uint16_t* sel = (volatile uint16_t*)(UINTN)(h->bar0 + QUEUE_SELECT);
        *sel = qsel;
    } else {
        uint16_t port = (uint16_t)(h->bar0 + QUEUE_SELECT);
        __asm__ volatile ("outw %0, %1" : : "a" (qsel), "dN" (port));
    }
    uint16_t qnum = 0;
    if (h->mmio) qnum = *(volatile uint16_t*)(UINTN)(h->bar0 + QUEUE_NUM);
    else { uint16_t port = (uint16_t)(h->bar0 + QUEUE_NUM); __asm__ volatile ("inw %1, %0" : "=a" (qnum) : "dN" (port)); }
    if (qnum == 0) return false;
    uint64_t addr = (uint64_t)(UINTN)pfn_mem;
    uint32_t pfn = (uint32_t)(addr / 4096ULL);
    if (h->mmio) {
        volatile uint32_t* pfn_reg = (volatile uint32_t*)(UINTN)(h->bar0 + QUEUE_PFN);
        *pfn_reg = pfn;
    } else {
        uint16_t port = (uint16_t)(h->bar0 + QUEUE_PFN);
        uint32_t v = pfn;
        __asm__ volatile ("outl %0, %1" : : "a" (v), "dN" (port));
    }
    return true;
}

// Helper: notify device (best-effort). Legacy spec uses QUEUE_NOTIFY at different offsets; try common offsets.
static void notify_device_local(const virtio_common::DeviceHandle* h, uint16_t queue_index) {
    uint16_t v = queue_index;
    uint32_t TRY_OFFSETS[] = {0x12, 0x10};
    for (int i = 0; i < 2; ++i) {
        uint32_t off = TRY_OFFSETS[i];
        if (h->mmio) {
            volatile uint16_t* p = (volatile uint16_t*)(UINTN)(h->bar0 + off);
            *p = v;
        } else {
            uint16_t port = (uint16_t)(h->bar0 + off);
            __asm__ volatile ("outw %0, %1" : : "a" (v), "dN" (port));
        }
    }
}

bool virtio_net::init() {
    if (g_ready) return true;
    if (!virtio_common::probe_device(virtio_common::DeviceType::NETWORK, &g_net_handle)) {
        Print(L"virtio-net: no device found\n");
        return false;
    }
    if (!virtio_common::device_init(&g_net_handle)) {
        Print(L"virtio-net: device_init failed\n");
        return false;
    }

    // Attempt basic feature negotiation (example: no features requested yet)
    // We'll read host features and write zero guest features for conservative start
    extern uint32_t virtio_common::read_host_features(void* bar0, bool mmio);
    extern bool virtio_common::negotiate_features(void* bar0, bool mmio, uint32_t want_mask);
    uint32_t host_feats = virtio_common::read_host_features((void*)(UINTN)g_net_handle.bar0, g_net_handle.mmio);
    (void)host_feats;
    virtio_common::negotiate_features((void*)(UINTN)g_net_handle.bar0, g_net_handle.mmio, 0u);

    // allocate queue memory for rx and tx
    size_t perq = (sizeof(VirtqDesc) * g_qsize) + (sizeof(uint16_t) * g_qsize) + (sizeof(VirtqUsedElem) * g_qsize) + 4096;
    g_rx_mem = dma::alloc(perq, 4096);
    g_tx_mem = dma::alloc(perq, 4096);
    if (!g_rx_mem || !g_tx_mem) {
        Print(L"virtio-net: DMA alloc failed\n");
        return false;
    }

    uint32_t desc_count = 0;
    g_rx_desc = virtqueue::alloc_virtqueue(g_rx_mem, g_qsize, &desc_count);
    g_tx_desc = virtqueue::alloc_virtqueue(g_tx_mem, g_qsize, &desc_count);
    if (!g_rx_desc || !g_tx_desc) {
        Print(L"virtio-net: virtqueue alloc failed\n");
        return false;
    }

    g_rx_vq = virtqueue_ops::view_from_mem(g_rx_mem, g_qsize);
    g_tx_vq = virtqueue_ops::view_from_mem(g_tx_mem, g_qsize);
    virtqueue_ops::init_rings(&g_rx_vq);
    virtqueue_ops::init_rings(&g_tx_vq);

    // Program PFNs for queues 0 (rx) and 1 (tx) — best-effort
    program_queue_pfn_local(&g_net_handle, 0, g_rx_mem);
    program_queue_pfn_local(&g_net_handle, 1, g_tx_mem);

    // Populate RX buffers
    for (uint32_t i = 0; i < g_rx_vq.size; ++i) {
        void* buf = dma::alloc(1536, 64);
        if (!buf) break;
        virtqueue_ops::set_descriptor(&g_rx_vq, i, (uint64_t)(UINTN)buf, 1536, 2 /* VIRTQ_DESC_F_WRITE */, 0);
        virtqueue_ops::submit_descriptor(&g_rx_vq, i);
    }

    // Notify device RX queue
    notify_device_local(&g_net_handle, 0);

    g_ready = true;
    Print(L"virtio-net: initialized (RX/TX queues allocated and RX buffers submitted)\n");
    return true;
}

bool virtio_net::is_available() { return g_ready; }

// Reclaim completed TX descriptors and recycle their buffers into pool
void virtio_net::reclaim_tx() {
    if (!g_ready) return;
    uint32_t id; uint32_t len;
    while (virtqueue_ops::try_dequeue_used(&g_tx_vq, &id, &len)) {
        uint64_t addr = g_tx_vq.desc[id].addr;
        void* buf = (void*)(UINTN)addr;
        // Mark descriptor as free
        virtqueue_ops::set_descriptor(&g_tx_vq, id, 0, 0, 0, 0);
        // Recycle buffer to pool
        tx_pool_push(buf);
    }
}

bool virtio_net::send_packet(const void* data, unsigned len) {
    if (!g_ready) return false;
    if (len > 1536) return false; // too big

    // Reclaim any completed TX first
    virtio_net::reclaim_tx();

    // Try to find a free descriptor index — simple linear scan for a zero-length descriptor
    uint32_t idx = UINT32_MAX;
    for (uint32_t i = 0; i < g_tx_vq.size; ++i) {
        if (g_tx_vq.desc[i].len == 0) { idx = i; break; }
    }
    if (idx == UINT32_MAX) return false;

    void* buf = tx_pool_pop();
    if (!buf) {
        buf = dma::alloc(len, 64);
        if (!buf) return false;
    }
    memcpy(buf, data, len);

    // Set descriptor and submit
    virtqueue_ops::set_descriptor(&g_tx_vq, idx, (uint64_t)(UINTN)buf, len, 0, 0);
    virtqueue_ops::submit_descriptor(&g_tx_vq, idx);
    // Notify device on TX queue (best-effort queue index 1)
    notify_device_local(&g_net_handle, 1);

    // Do not wait for completion here; reclaim_tx() will collect and recycle
    return true;
}

int virtio_net::receive_packet(void* buf, unsigned buf_len) {
    if (!g_ready) return -1;
    uint32_t id; uint32_t len;
    if (!virtqueue_ops::try_dequeue_used(&g_rx_vq, &id, &len)) return -1;
    if (len == 0 || len > buf_len) {
        // resubmit buffer
        uint64_t addr = g_rx_vq.desc[id].addr;
        virtqueue_ops::set_descriptor(&g_rx_vq, id, addr, 1536, 2, 0);
        virtqueue_ops::submit_descriptor(&g_rx_vq, id);
        notify_device_local(&g_net_handle, 0);
        return -1;
    }
    // copy packet
    uint64_t addr = g_rx_vq.desc[id].addr;
    memcpy(buf, (void*)(UINTN)addr, len);
    // resubmit buffer
    virtqueue_ops::set_descriptor(&g_rx_vq, id, addr, 1536, 2, 0);
    virtqueue_ops::submit_descriptor(&g_rx_vq, id);
    notify_device_local(&g_net_handle, 0);
    return (int)len;
}
