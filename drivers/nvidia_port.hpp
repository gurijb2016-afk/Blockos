#pragma once

#include <stddef.h>
#include <stdint.h>
#include "nvidia_probe.hpp"

namespace nvidia_port {

constexpr uint32_t OPS_ABI_VERSION = 1;
constexpr size_t PAGE_SIZE = 4096;
constexpr size_t MAX_DMA_ALLOCS = 16;
constexpr size_t MAX_FIRMWARE_BYTES = 32u * 1024u * 1024u;

enum class Status : uint8_t {
    Ok = 0,
    InvalidArgument,
    NotInitialized,
    AlreadyInitialized,
    NotFound,
    UnsupportedDevice,
    BadBar,
    MmioMapFailed,
    DmaFailed,
    IrqFailed,
    FirmwareNotFound,
    FirmwareTooLarge,
    FirmwareAllocationFailed,
    DeviceBusy,
    BadState,
    GspUnavailable,
    GspBootFailed,
    Timeout,
    InternalError
};

enum class CachePolicy : uint8_t { Uncached = 0, WriteCombining = 1 };
enum class DmaDirection : uint8_t { ToDevice = 0, FromDevice = 1, Bidirectional = 2 };
enum class DeviceState : uint8_t {
    Empty = 0,
    Discovered,
    MmioMapped,
    DmaPrepared,
    GspStarting,
    GspReady,
    Failed,
    Closed
};

struct PciAddress { uint8_t bus, slot, function; };

struct MmioMapping {
    volatile uint8_t* base;
    uint64_t physical_base;
    size_t size;
    void* cookie;
};

struct DmaBuffer {
    void* virtual_address;
    uint64_t device_address;
    size_t size;
    size_t alignment;
    void* cookie;
};

struct IrqRegistration {
    uint32_t vector;
    void* cookie;
};

using IrqHandler = void (*)(void* context);

/*
 * Kernel-owned primitives that are genuinely required by a GPU driver.
 * The callbacks must not allocate from an ordinary kernel heap for DMA:
 * dma_allocate must return memory that the PCI device can address and that
 * is coherent or correctly synchronized by dma_sync. The wrapper zeroes every
 * successful allocation before returning it.
 *
 * map_mmio must install the requested cache policy and return a valid virtual
 * mapping. irq_register must install a BlockOS-owned IDT/APIC route and call
 * handler(context) from interrupt context. The NVIDIA layer never guesses an
 * APIC vector, page-table layout, or physical address from a C pointer.
 */
struct PlatformOps {
    uint32_t abi_version;
    size_t struct_size;
    void* context;

    bool (*map_mmio)(void*, uint64_t physical, size_t size,
                     CachePolicy policy, volatile uint8_t** mapped,
                     void** cookie);
    void (*unmap_mmio)(void*, volatile uint8_t* mapped, size_t size,
                       void* cookie);

    bool (*dma_allocate)(void*, size_t size, size_t alignment,
                         uint64_t max_device_address, DmaBuffer* out);
    void (*dma_release)(void*, DmaBuffer* buffer);
    bool (*dma_sync)(void*, DmaBuffer* buffer, size_t offset,
                     size_t length, DmaDirection direction);

    bool (*irq_register)(void*, IrqHandler handler, void* handler_context,
                         IrqRegistration* out);
    void (*irq_unregister)(void*, IrqRegistration* registration);

    /* IRQ-save lock support. Must preserve/restore the local CPU interrupt flag. */
    uintptr_t (*irq_save_disable)(void*);
    void (*irq_restore)(void*, uintptr_t state);
    uint64_t (*monotonic_ns)(void*);
    void (*delay_us)(void*, uint32_t microseconds);

    /* Bridge to the upstream NVIDIA Resource Manager's GSP implementation.
     * These callbacks are deliberately explicit: the generic BlockOS port does
     * not invent undocumented GSP register offsets or wire formats. */
    bool (*gsp_boot)(void*, const PciAddress*, const MmioMapping*,
                     const DmaBuffer* firmware, size_t firmware_size,
                     DmaBuffer* bootstrap_scratch);
    bool (*gsp_service)(void*, const PciAddress*, const MmioMapping*,
                        uint32_t work_budget);
};

struct FirmwareImage {
    uint8_t* bytes;
    size_t size;
};

struct Device {
    DeviceState state;
    PciAddress pci;
    nvidia_probe::DeviceInfo id;
    uint16_t original_command;
    uint64_t bar0_base;
    uint64_t bar0_size;
    MmioMapping bar0_mapping;
    bool has_mapping;
    bool memory_decode_enabled;
    bool bus_master_enabled;
    DmaBuffer dma_allocations[MAX_DMA_ALLOCS];
    size_t dma_count;
    IrqRegistration irq;
    bool has_irq;
};

/* Ops install is intentionally one-shot; register before probing/attaching. */
Status install_platform_ops(const PlatformOps* ops);
bool platform_ready();
const char* status_string(Status status);

Status map_mmio(uint64_t physical, size_t size, CachePolicy policy, MmioMapping* out);
void unmap_mmio(MmioMapping* mapping);
bool mmio_read32(const MmioMapping* mapping, size_t offset, uint32_t* out);
bool mmio_write32(const MmioMapping* mapping, size_t offset, uint32_t value);

Status dma_allocate(size_t size, size_t alignment, uint64_t max_device_address,
                    DmaBuffer* out);
void dma_release(DmaBuffer* buffer);
bool dma_sync(DmaBuffer* buffer, size_t offset, size_t length, DmaDirection direction);

Status irq_register(IrqHandler handler, void* context, IrqRegistration* out);
void irq_unregister(IrqRegistration* registration);
uint64_t monotonic_ns();
void delay_us(uint32_t microseconds);
uintptr_t irq_save_disable();
void irq_restore(uintptr_t state);

/* Internal seam used by the device state machine. */
bool gsp_boot_available();
bool invoke_gsp_boot(const PciAddress*, const MmioMapping*,
                     DmaBuffer* firmware, size_t firmware_size,
                     DmaBuffer* bootstrap_scratch);
bool gsp_service_available();
bool invoke_gsp_service(const PciAddress*, const MmioMapping*, uint32_t work_budget);

Status load_firmware(const char* path, FirmwareImage* out);
void release_firmware(FirmwareImage* image);

/* PCI helpers use BlockOS's native pci/pcie implementation. */
Status enable_memory_decode(const PciAddress& address, uint16_t* original_command);
Status set_bus_master(const PciAddress& address, bool enabled);
void restore_pci_command(const PciAddress& address, uint16_t command);

/*
 * Device lifecycle: attach maps BAR0 and enables memory decode, but leaves bus
 * mastering disabled. DMA must be allocated and synchronized before calling
 * set_bus_master(). DMA handles returned by device_allocate_dma are borrowed
 * pointers into Device and become invalid at detach(). This is a safety boundary,
 * not a full NVIDIA RM init.
 */
Status attach_first(Device* out);
Status device_allocate_dma(Device* device, size_t size, size_t alignment,
                           uint64_t max_device_address, DmaBuffer** out);
Status device_enable_bus_master(Device* device);
Status device_register_irq(Device* device, IrqHandler handler, void* context);
Status device_boot_gsp(Device* device, const char* firmware_path);
Status device_service_gsp(Device* device, uint32_t work_budget);
void detach(Device* device);

} // namespace nvidia_port
