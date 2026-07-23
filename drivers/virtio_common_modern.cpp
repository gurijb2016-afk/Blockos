#include "virtio_common_modern.hpp"
#include <efi.h>
#include <efilib.h>
#include <string.h>

// This file implements a conservative, best-effort approach for modern virtio 1.0 (MMIO) devices.
// It will fall back to legacy behavior if modern registers are not present.

static inline uint32_t read32_mmio(uint64_t base, uint32_t off) {
    return *(volatile uint32_t*)(UINTN)(base + off);
}
static inline void write32_mmio(uint64_t base, uint32_t off, uint32_t v) {
    *(volatile uint32_t*)(UINTN)(base + off) = v;
}
static inline uint64_t read64_mmio(uint64_t base, uint32_t off) {
    return *(volatile uint64_t*)(UINTN)(base + off);
}
static inline void write64_mmio(uint64_t base, uint32_t off, uint64_t v) {
    *(volatile uint64_t*)(UINTN)(base + off) = v;
}

// Offsets based on Virtio 1.0 MMIO spec (conservative selection)
static const uint32_t VTIO_MMIO_MAGIC = 0x000; // 0..3 = 'virt'
static const uint32_t VTIO_MMIO_VERSION = 0x004; // version
static const uint32_t VTIO_MMIO_DEVICE_ID = 0x008;
static const uint32_t VTIO_MMIO_VENDOR_ID = 0x00c;
static const uint32_t VTIO_MMIO_DEVICE_FEATURES = 0x010; // 32-bit (low)
static const uint32_t VTIO_MMIO_DEVICE_FEATURES_SEL = 0x014;
static const uint32_t VTIO_MMIO_DRIVER_FEATURES = 0x020; // 32-bit (low)
static const uint32_t VTIO_MMIO_QUEUE_SEL = 0x030;
static const uint32_t VTIO_MMIO_QUEUE_NUM_MAX = 0x034;
static const uint32_t VTIO_MMIO_QUEUE_DESC_LOW = 0x080;
static const uint32_t VTIO_MMIO_QUEUE_DESC_HIGH = 0x084;
static const uint32_t VTIO_MMIO_QUEUE_AVAIL_LOW = 0x090;
static const uint32_t VTIO_MMIO_QUEUE_AVAIL_HIGH = 0x094;
static const uint32_t VTIO_MMIO_QUEUE_USED_LOW = 0x0a0;
static const uint32_t VTIO_MMIO_QUEUE_USED_HIGH = 0x0a4;

bool virtio_common::negotiate_modern_features(virtio_common::DeviceHandle* h, uint64_t want_mask_low) {
    if (!h || !h->mmio) return false;
    uint32_t magic = read32_mmio(h->bar0, VTIO_MMIO_MAGIC);
    // magic should be ASCII 'virt' (0x74726976)
    if (magic != 0x74726976u) {
        // not a modern MMIO virtio device
        return false;
    }

    uint32_t ver = read32_mmio(h->bar0, VTIO_MMIO_VERSION);
    CHAR16 buf[128];
    UnicodeSPrint(buf, sizeof(buf), L"virtio_common: modern MMIO device detected version=%u\n", ver);
    Print(buf);

    // Read device features (low 32 bits)
    write32_mmio(h->bar0, VTIO_MMIO_DEVICE_FEATURES_SEL, 0);
    uint32_t dev_feats0 = read32_mmio(h->bar0, VTIO_MMIO_DEVICE_FEATURES);
    write32_mmio(h->bar0, VTIO_MMIO_DEVICE_FEATURES_SEL, 1);
    uint32_t dev_feats1 = read32_mmio(h->bar0, VTIO_MMIO_DEVICE_FEATURES);
    uint64_t dev_features = ((uint64_t)dev_feats1 << 32) | (uint64_t)dev_feats0;

    // Decide guest features (only support low 32 bits mask)
    uint64_t agree = dev_features & (uint64_t)want_mask_low;
    uint32_t agree0 = (uint32_t)(agree & 0xFFFFFFFFu);
    write32_mmio(h->bar0, VTIO_MMIO_DRIVER_FEATURES, agree0);

    UnicodeSPrint(buf, sizeof(buf), L"virtio_common: device_features_low=0x%08x agreed=0x%08x\n", dev_feats0, agree0);
    Print(buf);
    return true;
}

bool virtio_common::program_modern_queue_addr(virtio_common::DeviceHandle* h, uint16_t queue_index, uint64_t desc, uint64_t avail, uint64_t used) {
    if (!h || !h->mmio) return false;
    uint32_t magic = read32_mmio(h->bar0, VTIO_MMIO_MAGIC);
    if (magic != 0x74726976u) return false;

    // select queue
    write32_mmio(h->bar0, VTIO_MMIO_QUEUE_SEL, queue_index);
    // write addresses
    write32_mmio(h->bar0, VTIO_MMIO_QUEUE_DESC_LOW, (uint32_t)(desc & 0xFFFFFFFFu));
    write32_mmio(h->bar0, VTIO_MMIO_QUEUE_DESC_HIGH, (uint32_t)(desc >> 32));
    write32_mmio(h->bar0, VTIO_MMIO_QUEUE_AVAIL_LOW, (uint32_t)(avail & 0xFFFFFFFFu));
    write32_mmio(h->bar0, VTIO_MMIO_QUEUE_AVAIL_HIGH, (uint32_t)(avail >> 32));
    write32_mmio(h->bar0, VTIO_MMIO_QUEUE_USED_LOW, (uint32_t)(used & 0xFFFFFFFFu));
    write32_mmio(h->bar0, VTIO_MMIO_QUEUE_USED_HIGH, (uint32_t)(used >> 32));

    CHAR16 buf[128];
    UnicodeSPrint(buf, sizeof(buf), L"virtio_common: programmed modern queue %u desc=0x%lx avail=0x%lx used=0x%lx\n",
                 queue_index, desc, avail, used);
    Print(buf);
    return true;
}
