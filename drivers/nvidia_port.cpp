#include "nvidia_port.hpp"
#include "drivers/pci.hpp"
#include "drivers/pcie.hpp"
#include "fs/vfs.hpp"
#include "kernel/allocator.hpp"

namespace nvidia_port {
namespace {
PlatformOps g_ops{};
bool g_ops_installed = false;

bool power_of_two(size_t v) { return v != 0 && (v & (v - 1)) == 0; }
bool add_overflow_u64(uint64_t a, uint64_t b, uint64_t* out)
{
    if (!out || UINT64_MAX - a < b) return true;
    *out = a + b;
    return false;
}
bool valid_address(const PciAddress& a)
{
    return a.slot < 32 && a.function < 8;
}
}

Status install_platform_ops(const PlatformOps* ops)
{
    if (!ops || ops->abi_version != OPS_ABI_VERSION ||
        ops->struct_size < sizeof(PlatformOps) ||
        !ops->map_mmio || !ops->unmap_mmio ||
        !ops->dma_allocate || !ops->dma_release || !ops->dma_sync ||
        !ops->irq_register || !ops->irq_unregister ||
        !ops->irq_save_disable || !ops->irq_restore ||
        !ops->monotonic_ns || !ops->delay_us)
        return Status::InvalidArgument;
    if (g_ops_installed) return Status::AlreadyInitialized;
    g_ops = *ops;
    g_ops_installed = true;
    return Status::Ok;
}

bool platform_ready() { return g_ops_installed; }

Status map_mmio(uint64_t physical, size_t size, CachePolicy policy, MmioMapping* out)
{
    uint64_t end = 0;
    if (!out || physical == 0 || size < sizeof(uint32_t) ||
        add_overflow_u64(physical, static_cast<uint64_t>(size) - 1, &end))
        return Status::InvalidArgument;
    if (!g_ops_installed) return Status::NotInitialized;
    volatile uint8_t* mapped = nullptr;
    void* cookie = nullptr;
    if (!g_ops.map_mmio(g_ops.context, physical, size, policy, &mapped, &cookie) || !mapped)
        return Status::MmioMapFailed;
    out->base = mapped;
    out->physical_base = physical;
    out->size = size;
    out->cookie = cookie;
    return Status::Ok;
}

void unmap_mmio(MmioMapping* mapping)
{
    if (!mapping || !mapping->base || !g_ops_installed) return;
    g_ops.unmap_mmio(g_ops.context, mapping->base, mapping->size, mapping->cookie);
    *mapping = {};
}

bool mmio_read32(const MmioMapping* mapping, size_t offset, uint32_t* out)
{
    if (!mapping || !mapping->base || !out || (offset & 3u) != 0 ||
        offset > mapping->size || mapping->size - offset < sizeof(uint32_t)) return false;
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    *out = *reinterpret_cast<volatile uint32_t*>(mapping->base + offset);
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
    return true;
}

bool mmio_write32(const MmioMapping* mapping, size_t offset, uint32_t value)
{
    if (!mapping || !mapping->base || (offset & 3u) != 0 ||
        offset > mapping->size || mapping->size - offset < sizeof(uint32_t)) return false;
    __atomic_thread_fence(__ATOMIC_RELEASE);
    *reinterpret_cast<volatile uint32_t*>(mapping->base + offset) = value;
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    return true;
}

Status dma_allocate(size_t size, size_t alignment, uint64_t max_device_address, DmaBuffer* out)
{
    uint64_t end = 0;
    if (!out || size == 0 || !power_of_two(alignment) || alignment < sizeof(void*))
        return Status::InvalidArgument;
    *out = {};
    if (!g_ops_installed) return Status::NotInitialized;
    DmaBuffer b{};
    if (!g_ops.dma_allocate(g_ops.context, size, alignment, max_device_address, &b))
        return Status::DmaFailed;
    const bool invalid = !b.virtual_address || b.size < size ||
        (b.device_address & static_cast<uint64_t>(alignment - 1)) != 0 ||
        add_overflow_u64(b.device_address, static_cast<uint64_t>(size) - 1, &end) ||
        end > max_device_address ||
        (reinterpret_cast<uintptr_t>(b.virtual_address) & (alignment - 1)) != 0;
    if (invalid) {
        g_ops.dma_release(g_ops.context, &b);
        return Status::DmaFailed;
    }
    // Do not expose stale kernel data to a device through DMA. Initialize the
    // requested bytes after validation and before publishing the handle.
    auto* bytes = static_cast<uint8_t*>(b.virtual_address);
    for (size_t i = 0; i < size; ++i) bytes[i] = 0;
    b.size = size;
    b.alignment = alignment;
    *out = b;
    return Status::Ok;
}

void dma_release(DmaBuffer* buffer)
{
    if (!buffer || !buffer->virtual_address || !g_ops_installed) return;
    g_ops.dma_release(g_ops.context, buffer);
    *buffer = {};
}

bool dma_sync(DmaBuffer* buffer, size_t offset, size_t length, DmaDirection direction)
{
    if (!buffer || !buffer->virtual_address || !g_ops_installed ||
        offset > buffer->size || length > buffer->size - offset) return false;
    if (length == 0) return true;
    return g_ops.dma_sync(g_ops.context, buffer, offset, length, direction);
}

Status irq_register(IrqHandler handler, void* context, IrqRegistration* out)
{
    if (!handler || !out) return Status::InvalidArgument;
    *out = {};
    if (!g_ops_installed) return Status::NotInitialized;
    if (!g_ops.irq_register(g_ops.context, handler, context, out)) return Status::IrqFailed;
    if (out->vector < 32 && !out->cookie) {
        g_ops.irq_unregister(g_ops.context, out);
        *out = {};
        return Status::IrqFailed;
    }
    return Status::Ok;
}

void irq_unregister(IrqRegistration* registration)
{
    if (!registration || (registration->vector == 0 && !registration->cookie) || !g_ops_installed) return;
    g_ops.irq_unregister(g_ops.context, registration);
    *registration = {};
}

uint64_t monotonic_ns() { return g_ops_installed ? g_ops.monotonic_ns(g_ops.context) : 0; }
void delay_us(uint32_t us) { if (g_ops_installed) g_ops.delay_us(g_ops.context, us); }
uintptr_t irq_save_disable() { return g_ops_installed ? g_ops.irq_save_disable(g_ops.context) : 0; }
void irq_restore(uintptr_t state) { if (g_ops_installed) g_ops.irq_restore(g_ops.context, state); }

bool gsp_boot_available() { return g_ops_installed && g_ops.gsp_boot != nullptr; }
bool invoke_gsp_boot(const PciAddress* pci, const MmioMapping* mmio,
                     DmaBuffer* firmware, size_t firmware_size,
                     DmaBuffer* scratch)
{
    return gsp_boot_available() && pci && mmio && firmware && scratch &&
        g_ops.gsp_boot(g_ops.context, pci, mmio, firmware, firmware_size, scratch);
}
bool gsp_service_available() { return g_ops_installed && g_ops.gsp_service != nullptr; }
bool invoke_gsp_service(const PciAddress* pci, const MmioMapping* mmio, uint32_t budget)
{
    return gsp_service_available() && pci && mmio && budget != 0 &&
        g_ops.gsp_service(g_ops.context, pci, mmio, budget);
}

Status load_firmware(const char* path, FirmwareImage* out)
{
    if (!path || !out) return Status::InvalidArgument;
    *out = {};
    uint32_t size32 = 0;
    const uint8_t* source = vfs::read_file(path, &size32);
    if (!source || size32 == 0) return Status::FirmwareNotFound;
    if (size32 > MAX_FIRMWARE_BYTES) return Status::FirmwareTooLarge;
    auto* copy = static_cast<uint8_t*>(allocator::alloc(size32, 16));
    if (!copy) return Status::FirmwareAllocationFailed;
    for (uint32_t i = 0; i < size32; ++i) copy[i] = source[i];
    out->bytes = copy;
    out->size = size32;
    return Status::Ok;
}

void release_firmware(FirmwareImage* image)
{
    if (!image || !image->bytes) return;
    allocator::free(image->bytes);
    *image = {};
}

Status enable_memory_decode(const PciAddress& a, uint16_t* original_command)
{
    if (!valid_address(a) || !original_command) return Status::InvalidArgument;
    if (!pci_device_exists(a.bus, a.slot, a.function)) return Status::NotFound;
    const uint16_t command = pci_cfg_read16(a.bus, a.slot, a.function, 0x04);
    *original_command = command;
    // Enable memory decoding but keep Bus Master disabled until DMA is ready.
    const uint16_t updated = static_cast<uint16_t>((command | 0x0002u) & ~0x0004u);
    pci_cfg_write16(a.bus, a.slot, a.function, 0x04, updated);
    const uint16_t verify = pci_cfg_read16(a.bus, a.slot, a.function, 0x04);
    if ((verify & 0x0002u) == 0 || (verify & 0x0004u) != 0) {
        pci_cfg_write16(a.bus, a.slot, a.function, 0x04, command);
        return Status::InternalError;
    }
    return Status::Ok;
}

Status set_bus_master(const PciAddress& a, bool enabled)
{
    if (!valid_address(a) || !pci_device_exists(a.bus, a.slot, a.function))
        return Status::InvalidArgument;
    uint16_t command = pci_cfg_read16(a.bus, a.slot, a.function, 0x04);
    if (enabled && !(command & 0x0002u)) return Status::BadState;
    command = enabled ? static_cast<uint16_t>(command | 0x0004u)
                      : static_cast<uint16_t>(command & ~0x0004u);
    pci_cfg_write16(a.bus, a.slot, a.function, 0x04, command);
    if (((pci_cfg_read16(a.bus, a.slot, a.function, 0x04) & 0x0004u) != 0) != enabled)
        return Status::InternalError;
    return Status::Ok;
}

void restore_pci_command(const PciAddress& a, uint16_t command)
{
    if (valid_address(a) && pci_device_exists(a.bus, a.slot, a.function))
        pci_cfg_write16(a.bus, a.slot, a.function, 0x04, command);
}

const char* status_string(Status s)
{
    switch (s) {
        case Status::Ok: return "ok";
        case Status::InvalidArgument: return "invalid argument";
        case Status::NotInitialized: return "platform operations not installed";
        case Status::AlreadyInitialized: return "platform operations already installed";
        case Status::NotFound: return "device not found";
        case Status::UnsupportedDevice: return "unsupported PCI device";
        case Status::BadBar: return "invalid BAR0";
        case Status::MmioMapFailed: return "MMIO mapping failed";
        case Status::DmaFailed: return "DMA allocation or validation failed";
        case Status::IrqFailed: return "interrupt registration failed";
        case Status::FirmwareNotFound: return "firmware file missing or empty";
        case Status::FirmwareTooLarge: return "firmware exceeds configured size limit";
        case Status::FirmwareAllocationFailed: return "firmware copy allocation failed";
        case Status::DeviceBusy: return "device is busy";
        case Status::BadState: return "invalid device state";
        case Status::GspUnavailable: return "GSP backend not installed";
        case Status::GspBootFailed: return "GSP boot failed";
        case Status::Timeout: return "operation timed out";
        case Status::InternalError: return "hardware or internal consistency error";
        default: return "unknown status";
    }
}

} // namespace nvidia_port
