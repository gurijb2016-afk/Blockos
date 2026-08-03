#include "virtio_net_driver.hpp"
#include "virtio_common.hpp"
#include "virtqueue.hpp"
#include "virtqueue_ops.hpp"
#include "dma.hpp"
#include "virtio_notify.hpp"
#include "virtio_service.hpp"

#include <efi.h>
#include <efilib.h>
#include <string.h>
#include <stdint.h>

namespace {

static bool g_ready = false;

static virtio_common::DeviceHandle g_device{};

static VirtQueueView g_tx_vq{};
static VirtQueueView g_rx_vq{};

static void* g_tx_mem = nullptr;
static void* g_rx_mem = nullptr;

static const uint32_t TX_QUEUE_SIZE = 128;
static const uint32_t RX_QUEUE_SIZE = 128;

struct TxSlot {
    void* buf;
    uint64_t submit_tick;
};

static TxSlot g_tx_slots[TX_QUEUE_SIZE]{};


/*
 * --------------------------------------------------------------------------
 * TX buffer recycling
 * --------------------------------------------------------------------------
 *
 * The current dma.hpp API does not expose a dma::free() function in the
 * code shown so far. Therefore this function is intentionally conservative.
 *
 * Once BlockOS has a DMA free API, it can be connected here.
 */
static void tx_pool_push(void* buf)
{
    if (buf == nullptr) {
        return;
    }

    /*
     * Do not call an unknown dma::free() API here.
     *
     * The buffer remains allocated for now. This avoids introducing another
     * compile error into the driver.
     */
    (void)buf;
}


static void clear_tx_slot(uint32_t id)
{
    if (id >= TX_QUEUE_SIZE) {
        return;
    }

    g_tx_slots[id].buf = nullptr;
    g_tx_slots[id].submit_tick = 0;
}


static void print_message(const CHAR16* message)
{
    Print(message);
}

} // anonymous namespace


namespace virtio_net {


bool init()
{
    g_ready = false;

    print_message(
        (const CHAR16*)L"virtio-net: initializing\n"
    );

    memset(
        &g_device,
        0,
        sizeof(g_device)
    );

    memset(
        &g_tx_vq,
        0,
        sizeof(g_tx_vq)
    );

    memset(
        &g_rx_vq,
        0,
        sizeof(g_rx_vq)
    );

    memset(
        g_tx_slots,
        0,
        sizeof(g_tx_slots)
    );

    /*
     * Queue memory allocation.
     *
     * The exact VirtIO queue memory size is normally calculated from
     * the queue size and alignment requirements. For this driver stage
     * allocate a conservative page-aligned region.
     */
    const size_t QUEUE_MEM_SIZE = 16384;

    g_tx_mem = dma::alloc(
        QUEUE_MEM_SIZE,
        4096
    );

    if (g_tx_mem == nullptr) {

        print_message(
            (const CHAR16*)
                L"virtio-net: TX queue DMA allocation failed\n"
        );

        return false;
    }

    g_rx_mem = dma::alloc(
        QUEUE_MEM_SIZE,
        4096
    );

    if (g_rx_mem == nullptr) {

        print_message(
            (const CHAR16*)
                L"virtio-net: RX queue DMA allocation failed\n"
        );

        return false;
    }

    memset(
        g_tx_mem,
        0,
        QUEUE_MEM_SIZE
    );

    memset(
        g_rx_mem,
        0,
        QUEUE_MEM_SIZE
    );

    /*
     * Create queue views using the actual BlockOS API.
     */
    g_tx_vq =
        virtqueue_ops::view_from_mem(
            g_tx_mem,
            TX_QUEUE_SIZE
        );

    g_rx_vq =
        virtqueue_ops::view_from_mem(
            g_rx_mem,
            RX_QUEUE_SIZE
        );

    if (g_tx_vq.desc == nullptr ||
        g_tx_vq.avail == nullptr ||
        g_tx_vq.used == nullptr) {

        print_message(
            (const CHAR16*)
                L"virtio-net: TX queue view creation failed\n"
        );

        return false;
    }

    if (g_rx_vq.desc == nullptr ||
        g_rx_vq.avail == nullptr ||
        g_rx_vq.used == nullptr) {

        print_message(
            (const CHAR16*)
                L"virtio-net: RX queue view creation failed\n"
        );

        return false;
    }

    virtqueue_ops::init_rings(
        &g_tx_vq
    );

    virtqueue_ops::init_rings(
        &g_rx_vq
    );

    print_message(
        (const CHAR16*)
            L"virtio-net: virtqueues initialized\n"
    );

    /*
     * The actual PCI VirtIO device negotiation must happen before the
     * driver can safely mark itself ready.
     *
     * Do not claim the device is ready yet.
     */
    g_ready = false;

    print_message(
        (const CHAR16*)
            L"virtio-net: waiting for device negotiation\n"
    );

    return true;
}


bool is_available()
{
    return g_ready;
}


bool send_packet(
    const void* data,
    unsigned len
)
{
    if (!g_ready) {
        return false;
    }

    if (data == nullptr || len == 0) {
        return false;
    }

    if (g_tx_vq.size == 0 ||
        g_tx_vq.desc == nullptr) {
        return false;
    }

    /*
     * First reclaim packets which the device has already processed.
     */
    reclaim_tx();

    uint32_t slot_count = TX_QUEUE_SIZE;

    if (g_tx_vq.size < slot_count) {
        slot_count = g_tx_vq.size;
    }

    /*
     * Find an unused software TX slot.
     */
    for (uint32_t i = 0; i < slot_count; ++i) {

        if (g_tx_slots[i].buf != nullptr) {
            continue;
        }

        /*
         * Allocate DMA-visible packet buffer.
         */
        void* tx_buf = dma::alloc(
            len,
            4096
        );

        if (tx_buf == nullptr) {

            print_message(
                (const CHAR16*)
                    L"virtio-net: TX buffer allocation failed\n"
            );

            return false;
        }

        memcpy(
            tx_buf,
            data,
            len
        );

        const uint64_t addr =
            static_cast<uint64_t>(
                reinterpret_cast<UINTN>(
                    tx_buf
                )
            );

        /*
         * VirtIO descriptor:
         *
         * addr  = DMA buffer
         * len   = packet length
         * flags = 0 for device-readable TX buffer
         * next  = 0
         */
        virtqueue_ops::set_descriptor(
            &g_tx_vq,
            i,
            addr,
            static_cast<uint32_t>(len),
            0,
            0
        );

        /*
         * Put descriptor into the available ring.
         */
        virtqueue_ops::submit_descriptor(
            &g_tx_vq,
            i
        );

        g_tx_slots[i].buf = tx_buf;

        g_tx_slots[i].submit_tick =
            virtio_service::now_ticks();

        /*
         * The exact notification mechanism depends on the configured
         * VirtIO transport. Do not invent a queue-specific notification
         * call here.
         */

        return true;
    }

    print_message(
        (const CHAR16*)
            L"virtio-net: no free TX descriptor\n"
    );

    return false;
}


int receive_packet(
    void* buf,
    unsigned buf_len
)
{
    if (!g_ready) {
        return -1;
    }

    if (buf == nullptr || buf_len == 0) {
        return -1;
    }

    if (g_rx_vq.size == 0 ||
        g_rx_vq.desc == nullptr) {
        return 0;
    }

    uint32_t id = 0;
    uint32_t len = 0;

    /*
     * Check whether the device completed an RX descriptor.
     */
    if (!virtqueue_ops::try_dequeue_used(
            &g_rx_vq,
            &id,
            &len)) {

        return 0;
    }

    if (id >= g_rx_vq.size) {
        return -1;
    }

    const uint64_t addr =
        g_rx_vq.desc[id].addr;

    if (addr == 0) {
        return -1;
    }

    void* rx_buf =
        reinterpret_cast<void*>(
            static_cast<UINTN>(addr)
        );

    unsigned copy_len = len;

    if (copy_len > buf_len) {
        copy_len = buf_len;
    }

    memcpy(
        buf,
        rx_buf,
        copy_len
    );

    /*
     * Clear descriptor after consuming it.
     */
    virtqueue_ops::set_descriptor(
        &g_rx_vq,
        id,
        0,
        0,
        0,
        0
    );

    return static_cast<int>(
        copy_len
    );
}


void reclaim_tx()
{
    if (!g_ready) {
        return;
    }

    uint32_t id = 0;
    uint32_t len = 0;

    /*
     * Reclaim every completed TX descriptor.
     */
    while (virtqueue_ops::try_dequeue_used(
        &g_tx_vq,
        &id,
        &len)) {

        (void)len;

        if (id >= g_tx_vq.size) {
            continue;
        }

        void* tracked_buf = nullptr;

        if (id < TX_QUEUE_SIZE) {
            tracked_buf =
                g_tx_slots[id].buf;
        }

        /*
         * Save descriptor address before clearing it.
         */
        const uint64_t addr =
            g_tx_vq.desc[id].addr;

        void* descriptor_buf =
            reinterpret_cast<void*>(
                static_cast<UINTN>(addr)
            );

        /*
         * Clear descriptor.
         */
        virtqueue_ops::set_descriptor(
            &g_tx_vq,
            id,
            0,
            0,
            0,
            0
        );

        /*
         * Prefer the software-tracked pointer.
         */
        if (tracked_buf != nullptr) {

            tx_pool_push(
                tracked_buf
            );

            clear_tx_slot(
                id
            );

        } else if (descriptor_buf != nullptr) {

            /*
             * Descriptor contained a buffer but software tracking did
             * not contain it.
             */
            tx_pool_push(
                descriptor_buf
            );
        }
    }

    /*
     * Timeout recovery.
     */
    const uint64_t now =
        virtio_service::now_ticks();

    static const uint64_t TIMEOUT_TICKS = 20;

    uint32_t max_slots = TX_QUEUE_SIZE;

    if (g_tx_vq.size < max_slots) {
        max_slots = g_tx_vq.size;
    }

    for (uint32_t i = 0; i < max_slots; ++i) {

        if (g_tx_slots[i].buf == nullptr) {
            continue;
        }

        const uint64_t submitted =
            g_tx_slots[i].submit_tick;

        if (now < submitted) {
            continue;
        }

        if ((now - submitted) <= TIMEOUT_TICKS) {
            continue;
        }

        CHAR16 msg[128];

        UnicodeSPrint(
            msg,
            sizeof(msg),
            (const CHAR16*)
                L"virtio-net: TX timeout slot %u\n",
            i
        );

        Print(msg);

        void* pending =
            g_tx_slots[i].buf;

        if (pending != nullptr) {
            tx_pool_push(
                pending
            );
        }

        clear_tx_slot(
            i
        );

        virtqueue_ops::set_descriptor(
            &g_tx_vq,
            i,
            0,
            0,
            0,
            0
        );
    }
}


} // namespace virtio_net
