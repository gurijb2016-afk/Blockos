#include "virtio_input.hpp"
#include "virtio_common.hpp"
#include "virtqueue.hpp"
#include "dma.hpp"
#include "events.hpp"
#include "pci.hpp"
#include <efi.h>
#include <efilib.h>
#include <string.h>

static bool inited = false;
static VirtqDesc* g_input_desc = nullptr;
static void* g_input_mem = nullptr;
static uint32_t g_input_qsize = 8;

// helper to program legacy virtio queue PFN
static bool program_queue_pfn(const virtio_common::DeviceHandle* h, uint16_t queue_sel, void* pfn_mem) {
    const uint32_t QUEUE_SELECT = 0x0C; // 16-bit
    const uint32_t QUEUE_NUM = 0x0E; // 16-bit
    const uint32_t QUEUE_PFN = 0x10; // 32-bit
    uint16_t qsel = (uint16_t)queue_sel;
    // select queue
    // write 16-bit
    if (h->mmio) {
        volatile uint16_t* sel = (volatile uint16_t*)(UINTN)(h->bar0 + QUEUE_SELECT);
        *sel = qsel;
    } else {
        // IO port access
        uint16_t port = (uint16_t)(h->bar0 + QUEUE_SELECT);
        __asm__ volatile ("outw %0, %1" : : "a" (qsel), "dN" (port));
    }
    uint16_t qnum;
    if (h->mmio) qnum = *(volatile uint16_t*)(UINTN)(h->bar0 + QUEUE_NUM);
    else { uint16_t port = (uint16_t)(h->bar0 + QUEUE_NUM); __asm__ volatile ("inw %1, %0" : "=a" (qnum) : "dN" (port)); }

    if (qnum == 0) return false;

    // Calculate PFN (page frame number) for the buffer — assume 4K pages
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

    // Program queue PFN for queue 0
    if (!program_queue_pfn(&h, 0, g_input_mem)) {
        Print(L"virtio_input: program_queue_pfn failed (device may not accept legacy queue programming)\n");
        // continue anyway; some devices may require different method (modern virtio)
    } else {
        Print(L"virtio_input: programmed queue PFN for queue 0\n");
    }

    // Try to set DRIVER_OK status bit
    const uint32_t STATUS = 0x12;
    uint8_t status = 0;
    if (h.mmio) status = *(volatile uint8_t*)(UINTN)(h.bar0 + STATUS);
    else { uint16_t port = (uint16_t)(h.bar0 + STATUS); __asm__ volatile ("inb %1, %0" : "=a" (status) : "dN" (port)); }
    status |= 4; // DRIVER_OK
    if (h.mmio) *(volatile uint8_t*)(UINTN)(h.bar0 + STATUS) = status;
    else { uint16_t port = (uint16_t)(h.bar0 + STATUS); __asm__ volatile ("outb %0, %1" : : "a" (status), "dN" (port)); }
    Print(L"virtio_input: set DRIVER_OK status bit\n");

    // Note: we still need to populate descriptors with buffers for receive and notify the device.
    // That will be implemented when integrating full virtqueue operations.

    Print(L"virtio_input: initialized (basic legacy queue programming attempted)\n");
    inited = true;
}

int8_t virtio_input::read_byte_nonblocking() {
    // TODO: poll virtqueue used ring for completed buffers and parse events.
    return INT8_MIN;
}
