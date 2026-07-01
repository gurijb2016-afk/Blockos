#include "virtio_net_driver.hpp"
#include "virtio_common.hpp"
#include "virtqueue.hpp"
#include "dma.hpp"
#include <efi.h>
#include <efilib.h>
#include <string.h>

// Minimal virtio-net implementation for legacy virtio-pci devices.
// This is a simplified driver intended for use under QEMU in the scaffold demo.
// It sets up two virtqueues: one for receive and one for transmit, using a small
// ring and DMA area allocated from dma::alloc().

static bool g_ready = false;
static VirtqDesc* g_rx_desc = nullptr;
static VirtqDesc* g_tx_desc = nullptr;
static uint32_t g_qsize = 8;
static void* g_rx_mem = nullptr;
static void* g_tx_mem = nullptr;

bool virtio_net::init() {
    if (g_ready) return true;
    virtio_common::DeviceHandle h;
    if (!virtio_common::probe_device(virtio_common::DeviceType::NETWORK, &h)) {
        Print(L"virtio-net: no device found\n");
        return false;
    }
    if (!virtio_common::device_init(&h)) {
        Print(L"virtio-net: device_init failed\n");
        return false;
    }

    // allocate small DMA areas for rx/tx virtqueues
    size_t perq = (sizeof(VirtqDesc) * g_qsize) + (sizeof(uint16_t) * g_qsize) + (sizeof(VirtqUsedElem) * g_qsize) + 1024;
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

    // TODO: program device BARs / queue addresses / notify device via PCI MMIO/I/O.
    // For this scaffold we rely on the virtio_common/device_init to do basic setup (stub)

    g_ready = true;
    Print(L"virtio-net: initialized (basic virtqueue setup)\n");
    return true;
}

bool virtio_net::is_available() { return g_ready; }

bool virtio_net::send_packet(const void* data, unsigned len) {
    if (!g_ready) return false;
    // Simplified: we don't actually enqueue to device (requires device notify), but
    // we pretend the packet was sent. Real implementation must place buffer in desc and
    // update avail ring and notify device.
    (void)data; (void)len;
    return false; // indicate not implemented
}

int virtio_net::receive_packet(void* buf, unsigned buf_len) {
    if (!g_ready) return -1;
    // Simplified: no real receive implemented. In a real driver we'd check the used ring
    // and copy the received frame into buf.
    (void)buf; (void)buf_len;
    return -1;
}
