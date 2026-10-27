#include "virtio_common.hpp"
#include "pci.hpp"
#include "dma.hpp"
#include <string.h>

extern "C" {
#include <efi.h>
}
extern "C" {
#include <efilib.h>
}

namespace {

constexpr uint16_t VIRTIO_VENDOR_ID = 0x1AF4;
constexpr uint16_t VIRTIO_NET_LEGACY = 0x1000;
constexpr uint16_t VIRTIO_BLK_LEGACY = 0x1001;
constexpr uint16_t VIRTIO_NET_MODERN = 0x1041;
constexpr uint16_t VIRTIO_BLK_MODERN = 0x1042;
constexpr uint16_t VIRTIO_GPU_LEGACY = 0x1010;
constexpr uint16_t VIRTIO_GPU_MODERN = 0x1050;
constexpr uint16_t VIRTIO_CONSOLE_LEGACY = 0x1003;
constexpr uint16_t VIRTIO_CONSOLE_MODERN = 0x1043;
constexpr uint16_t VIRTIO_RNG_LEGACY     = 0x1005;
constexpr uint16_t VIRTIO_RNG_MODERN     = 0x1044;
constexpr uint16_t VIRTIO_INPUT_LEGACY   = 0x1012;
constexpr uint16_t VIRTIO_INPUT_MODERN   = 0x1052;

constexpr uint8_t STATUS_ACKNOWLEDGE = 0x01;
constexpr uint8_t STATUS_DRIVER      = 0x02;
constexpr uint8_t STATUS_DRIVER_OK   = 0x04;
constexpr uint8_t STATUS_FEATURES_OK = 0x08;
constexpr uint8_t STATUS_FAILED      = 0x80;

constexpr uint64_t VIRTIO_F_VERSION_1 = 1ULL << 32;
constexpr uint64_t VIRTIO_F_RING_EVENT_IDX = 1ULL << 29;
constexpr uint64_t VIRTIO_NET_F_MAC = 1ULL << 5;
constexpr uint64_t VIRTIO_BLK_F_FLUSH = 1ULL << 9;

constexpr uint8_t PCI_CAP_ID_VENDOR = 0x09;
constexpr uint8_t VIRTIO_PCI_CAP_COMMON_CFG = 1;
constexpr uint8_t VIRTIO_PCI_CAP_NOTIFY_CFG = 2;
constexpr uint8_t VIRTIO_PCI_CAP_ISR_CFG = 3;
constexpr uint8_t VIRTIO_PCI_CAP_DEVICE_CFG = 4;

constexpr uint16_t LEG_HOST_FEATURES = 0x00;
constexpr uint16_t LEG_GUEST_FEATURES = 0x04;
constexpr uint16_t LEG_GUEST_PAGE_SIZE = 0x08;
constexpr uint16_t LEG_QUEUE_SELECT = 0x0C;
constexpr uint16_t LEG_QUEUE_SIZE = 0x0E;
constexpr uint16_t LEG_QUEUE_PFN = 0x10;
constexpr uint16_t LEG_QUEUE_NOTIFY = 0x10;
constexpr uint16_t LEG_STATUS = 0x12;
constexpr uint16_t LEG_CONFIG = 0x14;

constexpr uint16_t MOD_DEVICE_FEATURE_SELECT = 0x00;
constexpr uint16_t MOD_DEVICE_FEATURE = 0x04;
constexpr uint16_t MOD_DRIVER_FEATURE_SELECT = 0x08;
constexpr uint16_t MOD_DRIVER_FEATURE = 0x0C;
constexpr uint16_t MOD_DEVICE_STATUS = 0x14;
constexpr uint16_t MOD_QUEUE_SELECT = 0x16;
constexpr uint16_t MOD_QUEUE_SIZE = 0x18;
constexpr uint16_t MOD_QUEUE_ENABLE = 0x1C;
constexpr uint16_t MOD_QUEUE_NOTIFY_OFF = 0x1E;
constexpr uint16_t MOD_QUEUE_DESC = 0x20;
constexpr uint16_t MOD_QUEUE_DRIVER = 0x28;
constexpr uint16_t MOD_QUEUE_DEVICE = 0x30;

static inline void barrier()
{
    __asm__ volatile("mfence" ::: "memory");
}

static inline uint8_t io_in8(uint16_t p)
{
    uint8_t v;
    __asm__ volatile("inb %1,%0" : "=a"(v) : "dN"(p));
    return v;
}

static inline uint16_t io_in16(uint16_t p)
{
    uint16_t v;
    __asm__ volatile("inw %1,%0" : "=a"(v) : "dN"(p));
    return v;
}

static inline uint32_t io_in32(uint16_t p)
{
    uint32_t v;
    __asm__ volatile("inl %1,%0" : "=a"(v) : "dN"(p));
    return v;
}

static inline void io_out8(uint16_t p, uint8_t v)
{
    __asm__ volatile("outb %0,%1" : : "a"(v), "dN"(p));
}

static inline void io_out16(uint16_t p, uint16_t v)
{
    __asm__ volatile("outw %0,%1" : : "a"(v), "dN"(p));
}

static inline void io_out32(uint16_t p, uint32_t v)
{
    __asm__ volatile("outl %0,%1" : : "a"(v), "dN"(p));
}

static inline uint32_t mmio32(uint64_t a, uint32_t off)
{
    return *reinterpret_cast<volatile uint32_t*>(static_cast<UINTN>(a + off));
}

static inline uint16_t mmio16(uint64_t a, uint32_t off)
{
    return *reinterpret_cast<volatile uint16_t*>(static_cast<UINTN>(a + off));
}

static inline uint8_t mmio8(uint64_t a, uint32_t off)
{
    return *reinterpret_cast<volatile uint8_t*>(static_cast<UINTN>(a + off));
}

static inline void mmio32w(uint64_t a, uint32_t off, uint32_t v)
{
    *reinterpret_cast<volatile uint32_t*>(static_cast<UINTN>(a + off)) = v;
}

static inline void mmio16w(uint64_t a, uint32_t off, uint16_t v)
{
    *reinterpret_cast<volatile uint16_t*>(static_cast<UINTN>(a + off)) = v;
}

static inline void mmio8w(uint64_t a, uint32_t off, uint8_t v)
{
    *reinterpret_cast<volatile uint8_t*>(static_cast<UINTN>(a + off)) = v;
}

static inline uint32_t pci_bar_raw(uint8_t bus, uint8_t slot, uint8_t fn, int index)
{
    return pci_cfg_read32(bus, slot, fn, static_cast<uint8_t>(0x10 + index * 4));
}

static inline bool pci_bar_is_io(uint8_t bus, uint8_t slot, uint8_t fn, int index)
{
    return (pci_bar_raw(bus, slot, fn, index) & 1u) != 0;
}

static uint64_t pci_bar_base(uint8_t bus, uint8_t slot, uint8_t fn, int index)
{
    return pci_read_bar(bus, slot, fn, index);
}

static void enable_pci_bus_master(uint8_t bus, uint8_t slot, uint8_t fn)
{
    uint16_t cmd = pci_cfg_read16(bus, slot, fn, 0x04);
    cmd |= 0x0004; // Bus master
    cmd |= 0x0002; // Memory space
    pci_cfg_write16(bus, slot, fn, 0x04, cmd);
}

static uint8_t read_transport8(const virtio_common::DeviceHandle* h, uint32_t off)
{
    if (h->transport == virtio_common::Transport::PCI_IO)
        return io_in8(static_cast<uint16_t>(h->bar0 + off));
    return mmio8(h->bar0, off);
}

static uint16_t read_transport16(const virtio_common::DeviceHandle* h, uint32_t off)
{
    if (h->transport == virtio_common::Transport::PCI_IO)
        return io_in16(static_cast<uint16_t>(h->bar0 + off));
    return mmio16(h->bar0, off);
}

static uint32_t read_transport32(const virtio_common::DeviceHandle* h, uint32_t off)
{
    if (h->transport == virtio_common::Transport::PCI_IO)
        return io_in32(static_cast<uint16_t>(h->bar0 + off));
    return mmio32(h->bar0, off);
}

static void write_transport8(const virtio_common::DeviceHandle* h, uint32_t off, uint8_t v)
{
    if (h->transport == virtio_common::Transport::PCI_IO)
        io_out8(static_cast<uint16_t>(h->bar0 + off), v);
    else
        mmio8w(h->bar0, off, v);
}

static void write_transport16(const virtio_common::DeviceHandle* h, uint32_t off, uint16_t v)
{
    if (h->transport == virtio_common::Transport::PCI_IO)
        io_out16(static_cast<uint16_t>(h->bar0 + off), v);
    else
        mmio16w(h->bar0, off, v);
}

static void write_transport32(const virtio_common::DeviceHandle* h, uint32_t off, uint32_t v)
{
    if (h->transport == virtio_common::Transport::PCI_IO)
        io_out32(static_cast<uint16_t>(h->bar0 + off), v);
    else
        mmio32w(h->bar0, off, v);
}

static uint16_t common_read16(const virtio_common::DeviceHandle* h, uint32_t off)
{
    return mmio16(h->common_cfg, off);
}

static uint32_t common_read32(const virtio_common::DeviceHandle* h, uint32_t off)
{
    return mmio32(h->common_cfg, off);
}

static uint64_t common_read64(const virtio_common::DeviceHandle* h, uint32_t off)
{
    const uint32_t lo = mmio32(h->common_cfg, off);
    const uint32_t hi = mmio32(h->common_cfg, off + 4);
    return (static_cast<uint64_t>(hi) << 32) | lo;
}

static void common_write16(const virtio_common::DeviceHandle* h, uint32_t off, uint16_t v)
{
    mmio16w(h->common_cfg, off, v);
}

static void common_write32(const virtio_common::DeviceHandle* h, uint32_t off, uint32_t v)
{
    mmio32w(h->common_cfg, off, v);
}

static void common_write64(const virtio_common::DeviceHandle* h, uint32_t off, uint64_t v)
{
    mmio32w(h->common_cfg, off, static_cast<uint32_t>(v));
    mmio32w(h->common_cfg, off + 4, static_cast<uint32_t>(v >> 32));
}

static bool discover_capabilities(virtio_common::DeviceHandle* h)
{
    if (!h)
        return false;

    const uint8_t status = static_cast<uint8_t>(pci_cfg_read16(h->bus, h->slot, h->func, 0x06) >> 8);
    if ((status & 0x10) == 0)
        return false;

    uint8_t cap = pci_cfg_read8(h->bus, h->slot, h->func, 0x34);
    uint32_t notify_mult = 0;

    for (unsigned guard = 0; cap != 0 && guard < 64; ++guard) {
        if (cap < 0x40 || (cap & 3) != 0) {
            // PCI capability pointers are DWORD aligned and live in config space.
            cap = pci_cfg_read8(h->bus, h->slot, h->func, cap + 1);
            continue;
        }

        const uint8_t cap_id = pci_cfg_read8(h->bus, h->slot, h->func, cap + 0);
        const uint8_t next = pci_cfg_read8(h->bus, h->slot, h->func, cap + 1);

        if (cap_id == PCI_CAP_ID_VENDOR) {
            const uint8_t cfg_type = pci_cfg_read8(h->bus, h->slot, h->func, cap + 3);
            const uint8_t bar = pci_cfg_read8(h->bus, h->slot, h->func, cap + 4);
            const uint32_t offset = pci_cfg_read32(h->bus, h->slot, h->func, cap + 8);
            const uint32_t length = pci_cfg_read32(h->bus, h->slot, h->func, cap + 12);
            (void)length;

            if (bar < 6 && !pci_bar_is_io(h->bus, h->slot, h->func, bar)) {
                const uint64_t base = pci_bar_base(h->bus, h->slot, h->func, bar);
                if (base != 0) {
                    switch (cfg_type) {
                        case VIRTIO_PCI_CAP_COMMON_CFG:
                            h->common_cfg = base + offset;
                            break;
                        case VIRTIO_PCI_CAP_NOTIFY_CFG:
                            h->notify_cfg = base + offset;
                            notify_mult = pci_cfg_read32(h->bus, h->slot, h->func, cap + 16);
                            break;
                        case VIRTIO_PCI_CAP_ISR_CFG:
                            h->isr_cfg = base + offset;
                            break;
                        case VIRTIO_PCI_CAP_DEVICE_CFG:
                            h->device_cfg = base + offset;
                            break;
                        default:
                            break;
                    }
                }
            }
        }

        cap = next;
    }

    h->notify_off_multiplier = notify_mult;

    return h->common_cfg != 0 && h->notify_cfg != 0 && h->device_cfg != 0;
}

static bool find_device_ids(virtio_common::DeviceType type, uint16_t& legacy_id, uint16_t& modern_id)
{
    switch (type) {
        case virtio_common::DeviceType::NETWORK:
            legacy_id = VIRTIO_NET_LEGACY;
            modern_id = VIRTIO_NET_MODERN;
            return true;
        case virtio_common::DeviceType::BLOCK:
            legacy_id = VIRTIO_BLK_LEGACY;
            modern_id = VIRTIO_BLK_MODERN;
            return true;
        case virtio_common::DeviceType::GPU:
            legacy_id = VIRTIO_GPU_LEGACY;
            modern_id = VIRTIO_GPU_MODERN;
            return true;
        case virtio_common::DeviceType::INPUT:
            legacy_id = VIRTIO_INPUT_LEGACY;
            modern_id = VIRTIO_INPUT_MODERN;
            return true;
        case virtio_common::DeviceType::CONSOLE:
            legacy_id = VIRTIO_CONSOLE_LEGACY;
            modern_id = VIRTIO_CONSOLE_MODERN;
            return true;
        case virtio_common::DeviceType::ENTROPY:
            legacy_id = VIRTIO_RNG_LEGACY;
            modern_id = VIRTIO_RNG_MODERN;
            return true;
        default:
            return false;
    }
}

} // namespace

namespace virtio_common {

bool probe_device(DeviceType type, DeviceHandle* h)
{
    if (!h)
        return false;
    *h = DeviceHandle{};

    uint16_t legacy_id = 0;
    uint16_t modern_id = 0;
    if (!find_device_ids(type, legacy_id, modern_id))
        return false;

    for (uint32_t bus = 0; bus < 256; ++bus) {
        for (uint32_t slot = 0; slot < 32; ++slot) {
            uint16_t vendor0 = pci_cfg_read16(static_cast<uint8_t>(bus), static_cast<uint8_t>(slot), 0, 0x00);
            if (vendor0 == 0xFFFF)
                continue;
            uint8_t header = pci_cfg_read8(static_cast<uint8_t>(bus), static_cast<uint8_t>(slot), 0, 0x0E);
            uint8_t funcs = (header & 0x80) ? 8 : 1;
            for (uint32_t func = 0; func < funcs; ++func) {
                const uint8_t b = static_cast<uint8_t>(bus);
                const uint8_t s = static_cast<uint8_t>(slot);
                const uint8_t f = static_cast<uint8_t>(func);
                const uint16_t vendor = pci_cfg_read16(b, s, f, 0x00);
                const uint16_t device = pci_cfg_read16(b, s, f, 0x02);
                if (vendor != VIRTIO_VENDOR_ID || (device != legacy_id && device != modern_id))
                    continue;

                enable_pci_bus_master(b, s, f);

                h->device_id = device;
                h->bus = b;
                h->slot = s;
                h->func = f;
                h->vendor_id = vendor;
                h->irq = pci_cfg_read8(b, s, f, 0x3C);

                if (device == modern_id) {
                    h->modern = discover_capabilities(h);
                    if (h->modern) {
                        h->transport = Transport::PCI_MODERN;
                        h->mmio = true;
                        h->bar0 = h->common_cfg;
                        return true;
                    }
                }

                // Transitional/legacy transport: prefer an I/O BAR if present,
                // otherwise use a memory BAR as a legacy MMIO mapping.
                for (int bar = 0; bar < 6; ++bar) {
                    const uint64_t base = pci_bar_base(b, s, f, bar);
                    if (!base)
                        continue;
                    if (pci_bar_is_io(b, s, f, bar)) {
                        h->bar0 = base;
                        h->transport = Transport::PCI_IO;
                        h->mmio = false;
                    } else {
                        h->bar0 = base;
                        h->transport = Transport::PCI_MMIO;
                        h->mmio = true;
                    }
                    h->modern = false;
                    return true;
                }
            }
        }
    }

    return false;
}

void set_device_status(DeviceHandle* h, uint8_t status)
{
    if (!h)
        return;
    if (h->modern) {
        mmio8w(h->common_cfg, MOD_DEVICE_STATUS, status);
        return;
    }
    write_transport8(h, LEG_STATUS, status);
}

uint8_t get_device_status(DeviceHandle* h)
{
    if (!h)
        return 0;
    if (h->modern)
        return mmio8(h->common_cfg, MOD_DEVICE_STATUS);
    return read_transport8(h, LEG_STATUS);
}

static uint64_t read_legacy_features(DeviceHandle* h)
{
    return read_transport32(h, LEG_HOST_FEATURES);
}

static uint64_t read_modern_features(DeviceHandle* h)
{
    common_write32(h, MOD_DEVICE_FEATURE_SELECT, 0);
    const uint32_t lo = common_read32(h, MOD_DEVICE_FEATURE);
    common_write32(h, MOD_DEVICE_FEATURE_SELECT, 1);
    const uint32_t hi = common_read32(h, MOD_DEVICE_FEATURE);
    return (static_cast<uint64_t>(hi) << 32) | lo;
}

static bool write_modern_features(DeviceHandle* h, uint64_t features)
{
    common_write32(h, MOD_DRIVER_FEATURE_SELECT, 0);
    common_write32(h, MOD_DRIVER_FEATURE, static_cast<uint32_t>(features));
    common_write32(h, MOD_DRIVER_FEATURE_SELECT, 1);
    common_write32(h, MOD_DRIVER_FEATURE, static_cast<uint32_t>(features >> 32));
    barrier();
    return true;
}

bool negotiate_modern_features(DeviceHandle* h, uint64_t want_mask_low)
{
    if (!h || !h->modern)
        return false;

    h->host_features = read_modern_features(h);
    const uint64_t agreed = h->host_features & want_mask_low;
    if (!(agreed & VIRTIO_F_VERSION_1))
        return false;

    if (!write_modern_features(h, agreed))
        return false;

    h->negotiated_features = agreed;
    return true;
}

bool device_init(DeviceHandle* h, uint64_t wanted_features)
{
    if (!h)
        return false;

    set_device_status(h, 0);
    set_device_status(h, STATUS_ACKNOWLEDGE);
    set_device_status(h, STATUS_ACKNOWLEDGE | STATUS_DRIVER);

    if (h->modern) {
        const uint64_t required = VIRTIO_F_VERSION_1;
        const uint64_t mask = wanted_features | required;
        if (!negotiate_modern_features(h, mask)) {
            set_device_status(h, STATUS_FAILED);
            return false;
        }

        uint8_t status = get_device_status(h);
        status |= STATUS_FEATURES_OK;
        set_device_status(h, status);
        if (!(get_device_status(h) & STATUS_FEATURES_OK)) {
            set_device_status(h, STATUS_FAILED);
            return false;
        }

        return (get_device_status(h) & STATUS_FAILED) == 0;
    }

    h->host_features = read_legacy_features(h);
    const uint32_t agreed = static_cast<uint32_t>(h->host_features & static_cast<uint64_t>(wanted_features));
    write_transport32(h, LEG_GUEST_FEATURES, agreed);
    h->negotiated_features = agreed;
    write_transport32(h, LEG_GUEST_PAGE_SIZE, 4096);

    uint8_t status = get_device_status(h);
    status |= STATUS_FEATURES_OK;
    set_device_status(h, status);
    return (get_device_status(h) & STATUS_FAILED) == 0;
}

uint16_t queue_max_size(DeviceHandle* h, uint16_t queue_index)
{
    if (!h)
        return 0;

    if (h->modern) {
        common_write16(h, MOD_QUEUE_SELECT, queue_index);
        return common_read16(h, MOD_QUEUE_SIZE);
    }

    write_transport16(h, LEG_QUEUE_SELECT, queue_index);
    return read_transport16(h, LEG_QUEUE_SIZE);
}

bool setup_queue(
    DeviceHandle* h,
    uint16_t queue_index,
    void* memory,
    uint32_t requested_size,
    VirtQueueView* out_view)
{
    if (!h || !memory || !out_view || requested_size == 0)
        return false;

    const uintptr_t address = reinterpret_cast<uintptr_t>(memory);
    if ((address & 0xFFFu) != 0)
        return false;

    uint16_t max_size = queue_max_size(h, queue_index);
    if (max_size == 0)
        return false;

    uint32_t qsize = requested_size;
    if (qsize > max_size)
        qsize = max_size;
    if (qsize == 0)
        return false;

    if (h->modern) {
        common_write16(h, MOD_QUEUE_SELECT, queue_index);
        common_write16(h, MOD_QUEUE_SIZE, static_cast<uint16_t>(qsize));
        VirtQueueView view = virtqueue_ops::view_from_mem(memory, qsize);
        virtqueue_ops::init_rings(&view);
        common_write64(h, MOD_QUEUE_DESC, reinterpret_cast<uint64_t>(view.desc));
        common_write64(h, MOD_QUEUE_DRIVER, reinterpret_cast<uint64_t>(view.avail));
        common_write64(h, MOD_QUEUE_DEVICE, reinterpret_cast<uint64_t>(view.used));
        barrier();
        common_write16(h, MOD_QUEUE_ENABLE, 1);
        h->queue_sizes[queue_index < 8 ? queue_index : 0] = static_cast<uint16_t>(qsize);
        *out_view = view;
        return true;
    }

    write_transport16(h, LEG_QUEUE_SELECT, queue_index);
    write_transport16(h, LEG_QUEUE_SIZE, static_cast<uint16_t>(qsize));
    VirtQueueView view = virtqueue_ops::view_from_mem(memory, qsize);
    virtqueue_ops::init_rings(&view);
    write_transport32(h, LEG_QUEUE_PFN, static_cast<uint32_t>(address >> 12));
    h->queue_sizes[queue_index < 8 ? queue_index : 0] = static_cast<uint16_t>(qsize);
    *out_view = view;
    return true;
}

bool program_modern_queue_addr(
    DeviceHandle* h,
    uint16_t queue_index,
    uint64_t desc,
    uint64_t avail,
    uint64_t used)
{
    if (!h || !h->modern)
        return false;
    common_write16(h, MOD_QUEUE_SELECT, queue_index);
    common_write64(h, MOD_QUEUE_DESC, desc);
    common_write64(h, MOD_QUEUE_DRIVER, avail);
    common_write64(h, MOD_QUEUE_DEVICE, used);
    barrier();
    common_write16(h, MOD_QUEUE_ENABLE, 1);
    return true;
}

bool notify_queue(DeviceHandle* h, uint16_t queue_index)
{
    if (!h)
        return false;
    barrier();

    if (h->modern) {
        common_write16(h, MOD_QUEUE_SELECT, queue_index);
        const uint16_t off = common_read16(h, MOD_QUEUE_NOTIFY_OFF);
        const uint64_t addr = h->notify_cfg + static_cast<uint64_t>(off) * h->notify_off_multiplier;
        *reinterpret_cast<volatile uint16_t*>(static_cast<UINTN>(addr)) = queue_index;
        return true;
    }

    write_transport16(h, LEG_QUEUE_NOTIFY, queue_index);
    return true;
}

uint8_t read_device_config8(const DeviceHandle* h, uint32_t offset)
{
    if (!h) return 0;
    if (h->modern) return mmio8(h->device_cfg, offset);
    if (h->transport == Transport::PCI_IO) return io_in8(static_cast<uint16_t>(h->bar0 + LEG_CONFIG + offset));
    return mmio8(h->bar0, LEG_CONFIG + offset);
}

uint16_t read_device_config16(const DeviceHandle* h, uint32_t offset)
{
    if (!h) return 0;
    if (h->modern) return mmio16(h->device_cfg, offset);
    if (h->transport == Transport::PCI_IO) return io_in16(static_cast<uint16_t>(h->bar0 + LEG_CONFIG + offset));
    return mmio16(h->bar0, LEG_CONFIG + offset);
}

uint32_t read_device_config32(const DeviceHandle* h, uint32_t offset)
{
    if (!h) return 0;
    if (h->modern) return mmio32(h->device_cfg, offset);
    if (h->transport == Transport::PCI_IO) return io_in32(static_cast<uint16_t>(h->bar0 + LEG_CONFIG + offset));
    return mmio32(h->bar0, LEG_CONFIG + offset);
}

uint64_t read_device_config64(const DeviceHandle* h, uint32_t offset)
{
    if (!h) return 0;
    const uint64_t lo = read_device_config32(h, offset);
    const uint64_t hi = read_device_config32(h, offset + 4);
    return (hi << 32) | lo;
}

uint64_t block_capacity_sectors(const DeviceHandle* h)
{
    return read_device_config64(h, 0);
}

bool block_flush_supported(const DeviceHandle* h)
{
    return h && ((h->negotiated_features & VIRTIO_BLK_F_FLUSH) != 0);
}

} // namespace virtio_common
