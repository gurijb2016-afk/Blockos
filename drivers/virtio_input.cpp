#include "virtio_input.hpp"
#include "virtio_common.hpp"
#include "virtqueue.hpp"
#include "virtqueue_ops.hpp"
#include "dma.hpp"
#include "device_manager.hpp"

extern "C" {
#include <efi.h>
#include <efilib.h>
}

#include <stdint.h>
#include <string.h>

namespace virtio_input {
namespace {

constexpr uint16_t EVENT_QUEUE = 0;
constexpr uint16_t QUEUE_MAX_REQUESTED = 64;
constexpr uint8_t STATUS_DRIVER_OK = 0x04;
constexpr uint8_t STATUS_FAILED = 0x80;

static virtio_common::DeviceHandle g_handle{};
static VirtQueueView g_vq{};
static void* g_queue_mem = nullptr;
static VirtioInputEvent* g_event_buffers[QUEUE_MAX_REQUESTED]{};
static uint16_t g_queue_size = 0;
static bool g_ready = false;
static volatile uint32_t g_lock = 0;

static void lock()
{
    while (__atomic_test_and_set(&g_lock, __ATOMIC_ACQUIRE))
        __asm__ volatile("pause");
}

static void unlock()
{
    __atomic_clear(&g_lock, __ATOMIC_RELEASE);
}

static size_t queue_memory_size(uint16_t qsize)
{
    const size_t desc_size = sizeof(VirtqDesc) * qsize;
    const size_t avail_size = sizeof(uint16_t) * 2 + sizeof(uint16_t) * qsize;
    const size_t used_offset = (desc_size + avail_size + 4095u) & ~size_t(4095u);
    const size_t used_size = sizeof(uint16_t) * 2 + sizeof(VirtqUsedElem) * qsize;
    return used_offset + used_size + 4096;
}

static void fail_device()
{
    virtio_common::set_device_status(&g_handle, STATUS_FAILED);
}

} // namespace

bool init()
{
    lock();
    if (g_ready) {
        unlock();
        return true;
    }

    Print((CHAR16*)L"virtio-input: initializing native driver...\r\n");

    if (!virtio_common::probe_device(virtio_common::DeviceType::INPUT, &g_handle)) {
        Print((CHAR16*)L"virtio-input: no VirtIO PCI input device found\r\n");
        unlock();
        return false;
    }

    if (!virtio_common::device_init(&g_handle, 0)) {
        Print((CHAR16*)L"virtio-input: device initialization failed\r\n");
        fail_device();
        unlock();
        return false;
    }

    const uint16_t max_q = virtio_common::queue_max_size(&g_handle, EVENT_QUEUE);
    if (max_q < 2) {
        Print((CHAR16*)L"virtio-input: invalid event queue size\r\n");
        fail_device();
        unlock();
        return false;
    }

    g_queue_size = max_q > QUEUE_MAX_REQUESTED ? QUEUE_MAX_REQUESTED : max_q;
    const size_t mem_bytes = queue_memory_size(g_queue_size);
    g_queue_mem = dma::alloc(mem_bytes, 4096);
    if (!g_queue_mem) {
        Print((CHAR16*)L"virtio-input: queue DMA allocation failed\r\n");
        fail_device();
        unlock();
        return false;
    }
    memset(g_queue_mem, 0, mem_bytes);

    if (!virtio_common::setup_queue(&g_handle, EVENT_QUEUE, g_queue_mem, g_queue_size, &g_vq)) {
        Print((CHAR16*)L"virtio-input: setup_queue failed\r\n");
        fail_device();
        dma::free(g_queue_mem);
        g_queue_mem = nullptr;
        unlock();
        return false;
    }

    // Allocate receive buffers for incoming input events and post them to the queue
    for (uint16_t i = 0; i < g_queue_size; ++i) {
        VirtioInputEvent* buf = static_cast<VirtioInputEvent*>(dma::alloc(sizeof(VirtioInputEvent), 16));
        if (!buf) {
            Print((CHAR16*)L"virtio-input: event buffer allocation failed at %u\r\n", (UINTN)i);
            fail_device();
            unlock();
            return false;
        }
        memset(buf, 0, sizeof(VirtioInputEvent));
        g_event_buffers[i] = buf;

        // Flags = 2 (VIRTQ_DESC_F_WRITE: device writes into this buffer)
        virtqueue_ops::set_descriptor(&g_vq, i, reinterpret_cast<uint64_t>(buf), sizeof(VirtioInputEvent), 2, 0);
        virtqueue_ops::submit_descriptor(&g_vq, i);
    }

    __asm__ volatile("mfence" ::: "memory");
    const uint8_t status = virtio_common::get_device_status(&g_handle) | STATUS_DRIVER_OK;
    virtio_common::set_device_status(&g_handle, status);

    if ((virtio_common::get_device_status(&g_handle) & (STATUS_DRIVER_OK | STATUS_FAILED)) != STATUS_DRIVER_OK) {
        Print((CHAR16*)L"virtio-input: DRIVER_OK rejected by device\r\n");
        fail_device();
        unlock();
        return false;
    }

    virtio_common::notify_queue(&g_handle, EVENT_QUEUE);
    g_ready = true;

    // Register with BlockOS DeviceManager
    hardware_center.register_pci_device(
        "virtio-input",
        DEV_TYPE_INPUT,
        g_handle.bus, g_handle.slot, g_handle.func,
        g_handle.vendor_id, g_handle.device_id,
        0x09, 0x00, 0x00,
        g_handle.bar0, 0, g_handle.irq
    );

    Print((CHAR16*)L"virtio-input: successfully initialized (queue_size=%u)\r\n", (UINTN)g_queue_size);
    unlock();
    return true;
}

bool poll(VirtioInputEvent* event)
{
    if (!event || !g_ready)
        return false;

    lock();
    uint32_t used_id = 0;
    uint32_t used_len = 0;

    if (!virtqueue_ops::try_dequeue_used(&g_vq, &used_id, &used_len)) {
        unlock();
        return false;
    }

    if (used_id < g_queue_size && g_event_buffers[used_id] != nullptr) {
        *event = *g_event_buffers[used_id];

        // Re-submit the buffer descriptor back to the device to receive future events
        virtqueue_ops::set_descriptor(&g_vq, used_id,
                                      reinterpret_cast<uint64_t>(g_event_buffers[used_id]),
                                      sizeof(VirtioInputEvent), 2, 0);
        virtqueue_ops::submit_descriptor(&g_vq, used_id);
        virtio_common::notify_queue(&g_handle, EVENT_QUEUE);

        unlock();
        return true;
    }

    unlock();
    return false;
}

int8_t read_byte_nonblocking()
{
    VirtioInputEvent event{};
    if (!poll(&event))
        return INT8_MIN;

    // EV_KEY (type == 1) and key press/repeat (value >= 1)
    if (event.type == 1 && event.value >= 1) {
        // Return 7-bit keycode
        return static_cast<int8_t>(event.code & 0x7F);
    }

    return INT8_MIN;
}

} // namespace virtio_input
