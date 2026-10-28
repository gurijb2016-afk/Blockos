#include "nvidia_port.hpp"
#include "drivers/pci.hpp"
#include "drivers/pcie.hpp"

namespace nvidia_port {
namespace {
constexpr uint16_t NVIDIA_VENDOR = 0x10DE;
constexpr uint8_t DISPLAY_CLASS = 0x03;
constexpr size_t GSP_SCRATCH_BYTES = 64u * 1024u;

void zero_device(Device* d)
{
    if (!d) return;
    *d = {};
}

bool valid_device(const Device* d)
{
    return d && d->state != DeviceState::Empty && d->state != DeviceState::Closed;
}

DmaBuffer* allocate_tracked(Device* d, size_t size, size_t align, uint64_t max_addr)
{
    if (!d || d->dma_count >= MAX_DMA_ALLOCS) return nullptr;
    DmaBuffer* slot = &d->dma_allocations[d->dma_count];
    *slot = {};
    if (dma_allocate(size, align, max_addr, slot) != Status::Ok) return nullptr;
    ++d->dma_count;
    return slot;
}
}

Status attach_first(Device* out)
{
    if (!out) return Status::InvalidArgument;
    zero_device(out);
    if (!platform_ready()) return Status::NotInitialized;

    nvidia_probe::DeviceInfo info{};
    const nvidia_probe::Status probe_status = nvidia_probe::probe_first(&info);
    if (probe_status == nvidia_probe::Status::NotFound) return Status::NotFound;
    if (probe_status != nvidia_probe::Status::Found) return Status::BadBar;
    if (info.vendor_id != NVIDIA_VENDOR || info.class_code != DISPLAY_CLASS)
        return Status::UnsupportedDevice;

    const PciAddress address{info.bus, info.slot, info.function};
    // Size BAR0 only while PCI I/O, memory decoding, and Bus Master are off.
    // pcie::read_bar temporarily writes all ones to the BAR to obtain its mask.
    const uint16_t old_command = pci_cfg_read16(address.bus, address.slot, address.function, 0x04);
    const uint16_t disabled_command = static_cast<uint16_t>(old_command & ~0x0007u);
    pci_cfg_write16(address.bus, address.slot, address.function, 0x04, disabled_command);
    const uint16_t disabled_verify = pci_cfg_read16(address.bus, address.slot, address.function, 0x04);
    if (disabled_verify & 0x0007u) {
        pci_cfg_write16(address.bus, address.slot, address.function, 0x04, old_command);
        return Status::InternalError;
    }

    pcie::Bar bar{};
    if (!pcie::read_bar(address.bus, address.slot, address.function, 0, &bar) ||
        bar.is_io || bar.base == 0 || bar.size < PAGE_SIZE ||
        bar.size > 256u * 1024u * 1024u) {
        pci_cfg_write16(address.bus, address.slot, address.function, 0x04, old_command);
        return Status::BadBar;
    }

    // Enable MMIO decoding only; the device cannot issue DMA until the caller
    // has allocated DMA-capable buffers and explicitly enables Bus Master.
    const uint16_t memory_only = static_cast<uint16_t>((disabled_command | 0x0002u) & ~0x0004u);
    pci_cfg_write16(address.bus, address.slot, address.function, 0x04, memory_only);
    const uint16_t memory_verify = pci_cfg_read16(address.bus, address.slot, address.function, 0x04);
    if (!(memory_verify & 0x0002u) || (memory_verify & 0x0004u)) {
        pci_cfg_write16(address.bus, address.slot, address.function, 0x04, old_command);
        return Status::InternalError;
    }

    MmioMapping mapping{};
    Status s = map_mmio(bar.base, static_cast<size_t>(bar.size), CachePolicy::Uncached, &mapping);
    if (s != Status::Ok) {
        pci_cfg_write16(address.bus, address.slot, address.function, 0x04, old_command);
        return s;
    }

    out->state = DeviceState::MmioMapped;
    out->pci = address;
    out->id = info;
    out->original_command = old_command;
    out->bar0_base = bar.base;
    out->bar0_size = bar.size;
    out->bar0_mapping = mapping;
    out->has_mapping = true;
    out->memory_decode_enabled = true;
    out->bus_master_enabled = false;
    out->dma_count = 0;
    out->has_irq = false;
    return Status::Ok;
}

Status device_allocate_dma(Device* d, size_t size, size_t alignment,
                           uint64_t max_device_address, DmaBuffer** out)
{
    if (!out) return Status::InvalidArgument;
    *out = nullptr;
    if (!valid_device(d) || !d->has_mapping ||
        (d->state != DeviceState::MmioMapped && d->state != DeviceState::DmaPrepared))
        return Status::BadState;
    DmaBuffer* slot = allocate_tracked(d, size, alignment, max_device_address);
    if (!slot) return d->dma_count >= MAX_DMA_ALLOCS ? Status::DeviceBusy : Status::DmaFailed;
    d->state = DeviceState::DmaPrepared;
    *out = slot;
    return Status::Ok;
}

Status device_enable_bus_master(Device* d)
{
    if (!valid_device(d) || !d->has_mapping || d->dma_count == 0 ||
        (d->state != DeviceState::DmaPrepared && d->state != DeviceState::GspStarting))
        return Status::BadState;
    const Status s = set_bus_master(d->pci, true);
    if (s == Status::Ok) d->bus_master_enabled = true;
    return s;
}

Status device_register_irq(Device* d, IrqHandler handler, void* context)
{
    if (!valid_device(d) || !d->has_mapping) return Status::InvalidArgument;
    if (d->has_irq) return Status::AlreadyInitialized;
    IrqRegistration irq{};
    const Status s = irq_register(handler, context, &irq);
    if (s != Status::Ok) return s;
    d->irq = irq;
    d->has_irq = true;
    return Status::Ok;
}

Status device_boot_gsp(Device* d, const char* firmware_path)
{
    if (!valid_device(d) || !firmware_path ||
        !d->has_mapping || d->state != DeviceState::DmaPrepared)
        return Status::BadState;
    if (!platform_ready()) return Status::NotInitialized;

    // Never enable PCI bus mastering unless the NVIDIA RM/GSP protocol bridge
    // is installed. A firmware blob alone cannot initialize a GPU.
    if (!gsp_boot_available()) return Status::GspUnavailable;
    FirmwareImage image{};
    Status s = load_firmware(firmware_path, &image);
    if (s != Status::Ok) return s;

    DmaBuffer* firmware_dma = allocate_tracked(d, image.size, PAGE_SIZE, UINT64_MAX);
    if (!firmware_dma) {
        release_firmware(&image);
        return Status::DmaFailed;
    }
    auto* dst = static_cast<uint8_t*>(firmware_dma->virtual_address);
    for (size_t i = 0; i < image.size; ++i) dst[i] = image.bytes[i];
    const bool synced = dma_sync(firmware_dma, 0, image.size, DmaDirection::ToDevice);
    const size_t fw_size = image.size;
    release_firmware(&image);
    if (!synced) return Status::DmaFailed;

    DmaBuffer* scratch = allocate_tracked(d, GSP_SCRATCH_BYTES, PAGE_SIZE, UINT64_MAX);
    if (!scratch) return Status::DmaFailed;
    const Status bus_master_status = device_enable_bus_master(d);
    if (bus_master_status != Status::Ok) return bus_master_status;
    d->state = DeviceState::GspStarting;

    // platform.cpp exposes a guarded call into the target's NVIDIA RM binding.
    if (!gsp_boot_available()) {
        d->state = DeviceState::Failed;
        (void)set_bus_master(d->pci, false);
        d->bus_master_enabled = false;
        return Status::GspUnavailable;
    }
    if (!invoke_gsp_boot(&d->pci, &d->bar0_mapping, firmware_dma, fw_size, scratch)) {
        d->state = DeviceState::Failed;
        (void)set_bus_master(d->pci, false);
        d->bus_master_enabled = false;
        return Status::GspBootFailed;
    }
    d->state = DeviceState::GspReady;
    return Status::Ok;
}

Status device_service_gsp(Device* d, uint32_t work_budget)
{
    if (!valid_device(d) || d->state != DeviceState::GspReady || work_budget == 0)
        return Status::BadState;
    if (!gsp_service_available()) return Status::GspUnavailable;
    return invoke_gsp_service(&d->pci, &d->bar0_mapping, work_budget)
        ? Status::Ok : Status::InternalError;
}

void detach(Device* d)
{
    if (!d || d->state == DeviceState::Empty || d->state == DeviceState::Closed) return;
    // Disable bus mastering before unregistering DMA memory. This ordering is
    // essential: freeing pages while the GPU can still DMA is a use-after-free.
    if (d->bus_master_enabled) {
        (void)set_bus_master(d->pci, false);
        d->bus_master_enabled = false;
    }
    if (d->has_irq) {
        irq_unregister(&d->irq);
        d->has_irq = false;
    }
    while (d->dma_count != 0) {
        --d->dma_count;
        dma_release(&d->dma_allocations[d->dma_count]);
    }
    if (d->has_mapping) {
        unmap_mmio(&d->bar0_mapping);
        d->has_mapping = false;
    }
    if (d->memory_decode_enabled) {
        // Preserve the previous IO/memory decode settings, but keep bus master
        // disabled because buffers owned by this driver have been released.
        restore_pci_command(d->pci, static_cast<uint16_t>(d->original_command & ~0x0004u));
        d->memory_decode_enabled = false;
    }
    d->state = DeviceState::Closed;
}

} // namespace nvidia_port
