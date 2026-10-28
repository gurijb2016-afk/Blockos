#pragma once
#include "drivers/nvidia_dma_pool.hpp"

namespace nvidia_port {

/*
 * Small BlockOS binding object. It supplies the kernel-specific services which
 * cannot safely be inferred from a heap pointer or PCI BAR: actual page-table
 * MMIO mapping and IDT/APIC interrupt routing. DMA comes from DmaPool below.
 */
struct BlockOSKernelServices {
    void* context;
    bool (*map_mmio)(void*, uint64_t, size_t, CachePolicy,
                     volatile uint8_t**, void**);
    void (*unmap_mmio)(void*, volatile uint8_t*, size_t, void*);
    bool (*irq_register)(void*, IrqHandler, void*, IrqRegistration*);
    void (*irq_unregister)(void*, IrqRegistration*);
    bool (*gsp_boot)(void*, const PciAddress*, const MmioMapping*,
                     const DmaBuffer*, size_t, DmaBuffer*);
    bool (*gsp_service)(void*, const PciAddress*, const MmioMapping*, uint32_t);
};

struct BlockOSBinding {
    DmaPool* dma_pool;
    BlockOSKernelServices kernel;
    uint64_t tsc_frequency_hz; // Calibrated invariant-TSC frequency from BlockOS.
};

Status make_blockos_platform_ops(BlockOSBinding* binding, PlatformOps* out);

} // namespace nvidia_port
