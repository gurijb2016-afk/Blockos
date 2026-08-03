#include "virtio_common.hpp"
#include "pci.hpp"
#include "dma.hpp"

#include <efi.h>
#include <efilib.h>

#include <stdint.h>

// ============================================================
// I/O port helpers
// ============================================================

static inline void outb_io(uint16_t port, uint8_t val) {
    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"(val), "dN"(port)
    );
}

static inline uint8_t inb_io(uint16_t port) {
    uint8_t val;
    __asm__ volatile (
        "inb %1, %0"
        : "=a"(val)
        : "dN"(port)
    );
    return val;
}

static inline void outw_io(uint16_t port, uint16_t val) {
    __asm__ volatile (
        "outw %0, %1"
        :
        : "a"(val), "dN"(port)
    );
}

static inline uint16_t inw_io(uint16_t port) {
    uint16_t val;
    __asm__ volatile (
        "inw %1, %0"
        : "=a"(val)
        : "dN"(port)
    );
    return val;
}

static inline void outl_io(uint16_t port, uint32_t val) {
    __asm__ volatile (
        "outl %0, %1"
        :
        : "a"(val), "dN"(port)
    );
}

static inline uint32_t inl_io(uint16_t port) {
    uint32_t val;
    __asm__ volatile (
        "inl %1, %0"
        : "=a"(val)
        : "dN"(port)
    );
    return val;
}

// ============================================================
// MMIO helpers
// ============================================================

static uint8_t read_reg8_mmio(uint64_t base, uint32_t offset) {
    volatile uint8_t* p =
        (volatile uint8_t*)(UINTN)(base + offset);

    return *p;
}

static void write_reg8_mmio(
    uint64_t base,
    uint32_t offset,
    uint8_t value
) {
    volatile uint8_t* p =
        (volatile uint8_t*)(UINTN)(base + offset);

    *p = value;
}

static uint16_t read_reg16_mmio(uint64_t base, uint32_t offset) {
    volatile uint16_t* p =
        (volatile uint16_t*)(UINTN)(base + offset);

    return *p;
}

static void write_reg16_mmio(
    uint64_t base,
    uint32_t offset,
    uint16_t value
) {
    volatile uint16_t* p =
        (volatile uint16_t*)(UINTN)(base + offset);

    *p = value;
}

static uint32_t read_reg32_mmio(uint64_t base, uint32_t offset) {
    volatile uint32_t* p =
        (volatile uint32_t*)(UINTN)(base + offset);

    return *p;
}

static void write_reg32_mmio(
    uint64_t base,
    uint32_t offset,
    uint32_t value
) {
    volatile uint32_t* p =
        (volatile uint32_t*)(UINTN)(base + offset);

    *p = value;
}

// ============================================================
// Generic VirtIO register access
//
// Supports:
//   - legacy PCI I/O BAR
//   - MMIO BAR
// ============================================================

static uint8_t virtio_read8(
    const virtio_common::DeviceHandle* h,
    uint32_t offset
) {
    if (!h) {
        return 0;
    }

    if (h->mmio) {
        return read_reg8_mmio(h->bar0, offset);
    }

    return inb_io(
        (uint16_t)(h->bar0 + offset)
    );
}

static void virtio_write8(
    const virtio_common::DeviceHandle* h,
    uint32_t offset,
    uint8_t value
) {
    if (!h) {
        return;
    }

    if (h->mmio) {
        write_reg8_mmio(
            h->bar0,
            offset,
            value
        );
        return;
    }

    outb_io(
        (uint16_t)(h->bar0 + offset),
        value
    );
}

static uint16_t virtio_read16(
    const virtio_common::DeviceHandle* h,
    uint32_t offset
) {
    if (!h) {
        return 0;
    }

    if (h->mmio) {
        return read_reg16_mmio(
            h->bar0,
            offset
        );
    }

    return inw_io(
        (uint16_t)(h->bar0 + offset)
    );
}

static void virtio_write16(
    const virtio_common::DeviceHandle* h,
    uint32_t offset,
    uint16_t value
) {
    if (!h) {
        return;
    }

    if (h->mmio) {
        write_reg16_mmio(
            h->bar0,
            offset,
            value
        );
        return;
    }

    outw_io(
        (uint16_t)(h->bar0 + offset),
        value
    );
}

static uint32_t virtio_read32(
    const virtio_common::DeviceHandle* h,
    uint32_t offset
) {
    if (!h) {
        return 0;
    }

    if (h->mmio) {
        return read_reg32_mmio(
            h->bar0,
            offset
        );
    }

    return inl_io(
        (uint16_t)(h->bar0 + offset)
    );
}

static void virtio_write32(
    const virtio_common::DeviceHandle* h,
    uint32_t offset,
    uint32_t value
) {
    if (!h) {
        return;
    }

    if (h->mmio) {
        write_reg32_mmio(
            h->bar0,
            offset,
            value
        );
        return;
    }

    outl_io(
        (uint16_t)(h->bar0 + offset),
        value
    );
}

// ============================================================
// VirtIO legacy PCI initialization
// ============================================================

bool virtio_common::device_init(
    virtio_common::DeviceHandle* h
) {
    if (!h) {
        return false;
    }

    CHAR16 buf[256];

    // --------------------------------------------------------
    // Device information
    // --------------------------------------------------------

    UnicodeSPrint(
        buf,
        sizeof(buf),
        (CHAR16*)u"virtio_common: initializing device id=0x%08x at %d:%d.%d BAR0=0x%lx mmio=%d\n",
        h->device_id,
        h->bus,
        h->slot,
        h->func,
        h->bar0,
        h->mmio
    );

    Print(buf);

    // --------------------------------------------------------
    // Legacy VirtIO PCI register offsets
    // --------------------------------------------------------

    const uint32_t HOST_FEATURES    = 0x00;
    const uint32_t GUEST_FEATURES   = 0x04;
    const uint32_t GUEST_PAGE_SIZE  = 0x08;
    const uint32_t QUEUE_SELECT     = 0x0C;
    const uint32_t QUEUE_NUM        = 0x0E;
    const uint32_t QUEUE_PFN        = 0x10;
    const uint32_t STATUS           = 0x12;

    // --------------------------------------------------------
    // Legacy status bits
    // --------------------------------------------------------

    const uint8_t STATUS_ACK       = 1;
    const uint8_t STATUS_DRIVER    = 2;
    const uint8_t STATUS_DRIVER_OK = 4;

    (void)HOST_FEATURES;
    (void)GUEST_FEATURES;
    (void)QUEUE_SELECT;
    (void)QUEUE_NUM;
    (void)QUEUE_PFN;
    (void)STATUS_DRIVER_OK;

    // --------------------------------------------------------
    // Read current device status
    // --------------------------------------------------------

    uint8_t status = virtio_read8(
        h,
        STATUS
    );

    UnicodeSPrint(
        buf,
        sizeof(buf),
        (CHAR16*)u"virtio_common: current status=0x%02x\n",
        status
    );

    Print(buf);

    // --------------------------------------------------------
    // ACKNOWLEDGE device
    // --------------------------------------------------------

    status |= STATUS_ACK;

    virtio_write8(
        h,
        STATUS,
        status
    );

    // --------------------------------------------------------
    // Tell device that we are a driver
    // --------------------------------------------------------

    status |= STATUS_DRIVER;

    virtio_write8(
        h,
        STATUS,
        status
    );

    // --------------------------------------------------------
    // Read status back
    // --------------------------------------------------------

    uint8_t newstatus = virtio_read8(
        h,
        STATUS
    );

    UnicodeSPrint(
        buf,
        sizeof(buf),
        (CHAR16*)u"virtio_common: updated status=0x%02x\n",
        newstatus
    );

    Print(buf);

    // --------------------------------------------------------
    // Set guest page size
    //
    // Legacy VirtIO uses 4096-byte pages here.
    // --------------------------------------------------------

    virtio_write32(
        h,
        GUEST_PAGE_SIZE,
        4096
    );

    uint32_t gps = virtio_read32(
        h,
        GUEST_PAGE_SIZE
    );

    UnicodeSPrint(
        buf,
        sizeof(buf),
        (CHAR16*)u"virtio_common: guest_page_size=%u\n",
        gps
    );

    Print(buf);

    // --------------------------------------------------------
    // DMA allocation test
    //
    // This also verifies that the DMA subsystem is available
    // before a VirtIO driver attempts to create a queue.
    // --------------------------------------------------------

    void* dq = dma::alloc(
        4096
    );

    if (!dq) {

        Print(
            (CHAR16*)u"virtio_common: dma::alloc failed (virtqueue test)\n"
        );

    } else {

        UnicodeSPrint(
            buf,
            sizeof(buf),
            (CHAR16*)u"virtio_common: dma test alloc at %p\n",
            dq
        );

        Print(buf);
    }

    // --------------------------------------------------------
    // Do NOT set DRIVER_OK here.
    //
    // Individual VirtIO drivers should:
    //
    //   1. negotiate features
    //   2. select queue
    //   3. allocate queue
    //   4. program queue
    //   5. configure device
    //   6. finally set DRIVER_OK
    //
    // --------------------------------------------------------

    return true;
}


