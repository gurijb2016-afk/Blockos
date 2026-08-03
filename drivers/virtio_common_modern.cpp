#include "virtio_common_modern.hpp"

#include <efi.h>
#include <efilib.h>
#include <string.h>
#include <stdint.h>

// ============================================================
// VirtIO Modern MMIO helpers
// ============================================================

static inline uint32_t read32_mmio(
    uint64_t base,
    uint32_t offset
) {
    volatile uint32_t* p =
        (volatile uint32_t*)(UINTN)(base + offset);

    return *p;
}

static inline void write32_mmio(
    uint64_t base,
    uint32_t offset,
    uint32_t value
) {
    volatile uint32_t* p =
        (volatile uint32_t*)(UINTN)(base + offset);

    *p = value;
}

static inline uint64_t read64_mmio(
    uint64_t base,
    uint32_t offset
) {
    volatile uint64_t* p =
        (volatile uint64_t*)(UINTN)(base + offset);

    return *p;
}

static inline void write64_mmio(
    uint64_t base,
    uint32_t offset,
    uint64_t value
) {
    volatile uint64_t* p =
        (volatile uint64_t*)(UINTN)(base + offset);

    *p = value;
}

// ============================================================
// VirtIO MMIO register offsets
//
// VirtIO MMIO 1.0 / modern layout
// ============================================================

static const uint32_t VTIO_MMIO_MAGIC =
    0x000;

static const uint32_t VTIO_MMIO_VERSION =
    0x004;

static const uint32_t VTIO_MMIO_DEVICE_ID =
    0x008;

static const uint32_t VTIO_MMIO_VENDOR_ID =
    0x00C;

static const uint32_t VTIO_MMIO_DEVICE_FEATURES =
    0x010;

static const uint32_t VTIO_MMIO_DEVICE_FEATURES_SEL =
    0x014;

static const uint32_t VTIO_MMIO_DRIVER_FEATURES =
    0x020;

static const uint32_t VTIO_MMIO_QUEUE_SEL =
    0x030;

static const uint32_t VTIO_MMIO_QUEUE_NUM_MAX =
    0x034;

static const uint32_t VTIO_MMIO_QUEUE_DESC_LOW =
    0x080;

static const uint32_t VTIO_MMIO_QUEUE_DESC_HIGH =
    0x084;

static const uint32_t VTIO_MMIO_QUEUE_AVAIL_LOW =
    0x090;

static const uint32_t VTIO_MMIO_QUEUE_AVAIL_HIGH =
    0x094;

static const uint32_t VTIO_MMIO_QUEUE_USED_LOW =
    0x0A0;

static const uint32_t VTIO_MMIO_QUEUE_USED_HIGH =
    0x0A4;

// ============================================================
// VirtIO MMIO magic
// ============================================================
//
// ASCII:
//   'v' = 0x76
//   'i' = 0x69
//   'r' = 0x72
//   't' = 0x74
//
// Little endian uint32:
//   0x74726976
// ============================================================

static const uint32_t VIRTIO_MMIO_MAGIC_VALUE =
    0x74726976u;

// ============================================================
// Modern VirtIO feature negotiation
// ============================================================

bool virtio_common::negotiate_modern_features(
    virtio_common::DeviceHandle* h,
    uint64_t want_mask_low
) {
    if (!h) {
        return false;
    }

    if (!h->mmio) {
        return false;
    }

    // --------------------------------------------------------
    // Check MMIO magic
    // --------------------------------------------------------

    uint32_t magic =
        read32_mmio(
            h->bar0,
            VTIO_MMIO_MAGIC
        );

    if (magic != VIRTIO_MMIO_MAGIC_VALUE) {
        return false;
    }

    // --------------------------------------------------------
    // Read VirtIO MMIO version
    // --------------------------------------------------------

    uint32_t version =
        read32_mmio(
            h->bar0,
            VTIO_MMIO_VERSION
        );

    CHAR16 buf[256];

    UnicodeSPrint(
        buf,
        sizeof(buf),
        (CHAR16*)u"virtio_common: modern MMIO device detected version=%u\n",
        version
    );

    Print(buf);

    // --------------------------------------------------------
    // Read low 32-bit device features
    // --------------------------------------------------------

    write32_mmio(
        h->bar0,
        VTIO_MMIO_DEVICE_FEATURES_SEL,
        0
    );

    uint32_t device_features_low =
        read32_mmio(
            h->bar0,
            VTIO_MMIO_DEVICE_FEATURES
        );

    // --------------------------------------------------------
    // Read high 32-bit device features
    // --------------------------------------------------------

    write32_mmio(
        h->bar0,
        VTIO_MMIO_DEVICE_FEATURES_SEL,
        1
    );

    uint32_t device_features_high =
        read32_mmio(
            h->bar0,
            VTIO_MMIO_DEVICE_FEATURES
        );

    uint64_t device_features =
        ((uint64_t)device_features_high << 32) |
        (uint64_t)device_features_low;

    // --------------------------------------------------------
    // Select features supported by BlockOS
    //
    // Currently want_mask_low is used as the supported mask.
    // --------------------------------------------------------

    uint64_t agreed_features =
        device_features &
        want_mask_low;

    uint32_t agreed_features_low =
        (uint32_t)(
            agreed_features &
            0xFFFFFFFFULL
        );

    // --------------------------------------------------------
    // Write negotiated low feature bits
    // --------------------------------------------------------

    write32_mmio(
        h->bar0,
        VTIO_MMIO_DRIVER_FEATURES,
        agreed_features_low
    );

    // --------------------------------------------------------
    // Debug output
    // --------------------------------------------------------

    UnicodeSPrint(
        buf,
        sizeof(buf),
        (CHAR16*)u"virtio_common: device_features_low=0x%08x agreed=0x%08x\n",
        device_features_low,
        agreed_features_low
    );

    Print(buf);

    return true;
}

// ============================================================
// Program modern VirtIO queue addresses
// ============================================================

bool virtio_common::program_modern_queue_addr(
    virtio_common::DeviceHandle* h,
    uint16_t queue_index,
    uint64_t desc,
    uint64_t avail,
    uint64_t used
) {
    if (!h) {
        return false;
    }

    if (!h->mmio) {
        return false;
    }

    // --------------------------------------------------------
    // Verify MMIO magic
    // --------------------------------------------------------

    uint32_t magic =
        read32_mmio(
            h->bar0,
            VTIO_MMIO_MAGIC
        );

    if (magic != VIRTIO_MMIO_MAGIC_VALUE) {
        return false;
    }

    // --------------------------------------------------------
    // Select queue
    // --------------------------------------------------------

    write32_mmio(
        h->bar0,
        VTIO_MMIO_QUEUE_SEL,
        (uint32_t)queue_index
    );

    // --------------------------------------------------------
    // Descriptor table address
    // --------------------------------------------------------

    write32_mmio(
        h->bar0,
        VTIO_MMIO_QUEUE_DESC_LOW,
        (uint32_t)(
            desc & 0xFFFFFFFFULL
        )
    );

    write32_mmio(
        h->bar0,
        VTIO_MMIO_QUEUE_DESC_HIGH,
        (uint32_t)(
            desc >> 32
        )
    );

    // --------------------------------------------------------
    // Available ring address
    // --------------------------------------------------------

    write32_mmio(
        h->bar0,
        VTIO_MMIO_QUEUE_AVAIL_LOW,
        (uint32_t)(
            avail & 0xFFFFFFFFULL
        )
    );

    write32_mmio(
        h->bar0,
        VTIO_MMIO_QUEUE_AVAIL_HIGH,
        (uint32_t)(
            avail >> 32
        )
    );

    // --------------------------------------------------------
    // Used ring address
    // --------------------------------------------------------

    write32_mmio(
        h->bar0,
        VTIO_MMIO_QUEUE_USED_LOW,
        (uint32_t)(
            used & 0xFFFFFFFFULL
        )
    );

    write32_mmio(
        h->bar0,
        VTIO_MMIO_QUEUE_USED_HIGH,
        (uint32_t)(
            used >> 32
        )
    );

    // --------------------------------------------------------
    // Debug output
    // --------------------------------------------------------

    CHAR16 buf[256];

    UnicodeSPrint(
        buf,
        sizeof(buf),
        (CHAR16*)u"virtio_common: programmed modern queue %u desc=0x%lx avail=0x%lx used=0x%lx\n",
        queue_index,
        desc,
        avail,
        used
    );

    Print(buf);

    return true;
}
