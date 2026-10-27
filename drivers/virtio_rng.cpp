#include "virtio_rng.hpp"
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

namespace virtio_rng {
namespace {

constexpr uint16_t RNG_QUEUE = 0;
constexpr uint16_t QUEUE_SIZE_REQUESTED = 16;
constexpr uint8_t STATUS_DRIVER_OK = 0x04;
constexpr uint8_t STATUS_FAILED = 0x80;
constexpr size_t MAX_CHUNK = 512;

static virtio_common::DeviceHandle g_handle{};
static VirtQueueView g_vq{};
static void* g_queue_mem = nullptr;
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

    Print((CHAR16*)L"virtio-rng: initializing native entropy driver...\r\n");

    if (!virtio_common::probe_device(virtio_common::DeviceType::ENTROPY, &g_handle)) {
        Print((CHAR16*)L"virtio-rng: no VirtIO PCI RNG device found\r\n");
        unlock();
        return false;
    }

    if (!virtio_common::device_init(&g_handle, 0)) {
        Print((CHAR16*)L"virtio-rng: device initialization failed\r\n");
        fail_device();
        unlock();
        return false;
    }

    const uint16_t max_q = virtio_common::queue_max_size(&g_handle, RNG_QUEUE);
    if (max_q < 1) {
        Print((CHAR16*)L"virtio-rng: invalid queue size\r\n");
        fail_device();
        unlock();
        return false;
    }

    g_queue_size = max_q > QUEUE_SIZE_REQUESTED ? QUEUE_SIZE_REQUESTED : max_q;
    const size_t mem_bytes = queue_memory_size(g_queue_size);
    g_queue_mem = dma::alloc(mem_bytes, 4096);
    if (!g_queue_mem) {
        Print((CHAR16*)L"virtio-rng: queue DMA allocation failed\r\n");
        fail_device();
        unlock();
        return false;
    }
    memset(g_queue_mem, 0, mem_bytes);

    if (!virtio_common::setup_queue(&g_handle, RNG_QUEUE, g_queue_mem, g_queue_size, &g_vq)) {
        Print((CHAR16*)L"virtio-rng: setup_queue failed\r\n");
        fail_device();
        dma::free(g_queue_mem);
        g_queue_mem = nullptr;
        unlock();
        return false;
    }

    __asm__ volatile("mfence" ::: "memory");
    const uint8_t status = virtio_common::get_device_status(&g_handle) | STATUS_DRIVER_OK;
    virtio_common::set_device_status(&g_handle, status);

    if ((virtio_common::get_device_status(&g_handle) & (STATUS_DRIVER_OK | STATUS_FAILED)) != STATUS_DRIVER_OK) {
        Print((CHAR16*)L"virtio-rng: DRIVER_OK rejected by device\r\n");
        fail_device();
        unlock();
        return false;
    }

    g_ready = true;

    // Register with DeviceManager
    hardware_center.register_pci_device(
        "virtio-rng",
        DEV_TYPE_UNKNOWN,
        g_handle.bus, g_handle.slot, g_handle.func,
        g_handle.vendor_id, g_handle.device_id,
        0x08, 0x80, 0x00,
        g_handle.bar0, 0, g_handle.irq
    );

    Print((CHAR16*)L"virtio-rng: ready (entropy source online)\r\n");
    unlock();
    return true;
}

bool is_available()
{
    return g_ready;
}

bool get_random_bytes(void* buffer, size_t length)
{
    if (!buffer || length == 0 || !g_ready)
        return false;

    lock();
    uint8_t* dest = static_cast<uint8_t*>(buffer);
    size_t remaining = length;

    while (remaining > 0) {
        const size_t chunk = remaining > MAX_CHUNK ? MAX_CHUNK : remaining;

        void* dma_buf = dma::alloc(chunk, 16);
        if (!dma_buf) {
            unlock();
            return false;
        }

        // Set descriptor 0: device writes random bytes into dma_buf
        virtqueue_ops::set_descriptor(&g_vq, 0, reinterpret_cast<uint64_t>(dma_buf),
                                      static_cast<uint32_t>(chunk), 2 /* VIRTQ_DESC_F_WRITE */, 0);
        virtqueue_ops::submit_descriptor(&g_vq, 0);
        virtio_common::notify_queue(&g_handle, RNG_QUEUE);

        // Wait for device to populate random bytes
        uint32_t used_id = 0;
        uint32_t used_len = 0;
        bool completed = false;

        for (uint64_t spin = 0; spin < 2000000ULL; ++spin) {
            if (virtqueue_ops::try_dequeue_used(&g_vq, &used_id, &used_len)) {
                completed = true;
                break;
            }
            if ((spin & 0xFF) == 0)
                __asm__ volatile("pause");
        }

        if (!completed) {
            dma::free(dma_buf);
            unlock();
            return false;
        }

        const size_t bytes_to_copy = used_len < chunk ? used_len : chunk;
        memcpy(dest, dma_buf, bytes_to_copy);
        dma::free(dma_buf);

        dest += bytes_to_copy;
        remaining -= bytes_to_copy;

        if (bytes_to_copy == 0)
            break;
    }

    unlock();
    return remaining == 0;
}

uint32_t get_random_u32()
{
    uint32_t val = 0;
    if (get_random_bytes(&val, sizeof(val)))
        return val;

    // Fallback pseudo-random LCG if hardware not ready
    static uint32_t s_seed = 0x12345678;
    s_seed = s_seed * 1664525u + 1013904223u;
    return s_seed;
}

} // namespace virtio_rng
