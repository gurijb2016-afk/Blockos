#include "drivers/nvidia_blockos_binding.hpp"

namespace nvidia_port {
namespace {
uint64_t read_tsc()
{
    uint32_t lo = 0, hi = 0;
    asm volatile("lfence; rdtsc" : "=a"(lo), "=d"(hi) :: "memory");
    return (static_cast<uint64_t>(hi) << 32) | lo;
}

bool bind_map(void* c, uint64_t phys, size_t size, CachePolicy policy,
              volatile uint8_t** mapped, void** cookie)
{
    auto* b = static_cast<BlockOSBinding*>(c);
    return b && b->kernel.map_mmio &&
        b->kernel.map_mmio(b->kernel.context, phys, size, policy, mapped, cookie);
}
void bind_unmap(void* c, volatile uint8_t* mapped, size_t size, void* cookie)
{
    auto* b = static_cast<BlockOSBinding*>(c);
    if (b && b->kernel.unmap_mmio)
        b->kernel.unmap_mmio(b->kernel.context, mapped, size, cookie);
}
bool bind_dma_allocate(void* c, size_t size, size_t alignment,
                       uint64_t max_addr, DmaBuffer* out)
{
    auto* b = static_cast<BlockOSBinding*>(c);
    return b && b->dma_pool && dma_pool_allocate(b->dma_pool, size, alignment, max_addr, out);
}
void bind_dma_release(void* c, DmaBuffer* buffer)
{
    auto* b = static_cast<BlockOSBinding*>(c);
    if (b && b->dma_pool) dma_pool_release(b->dma_pool, buffer);
}
bool bind_dma_sync(void* c, DmaBuffer* buffer, size_t offset,
                   size_t length, DmaDirection direction)
{
    auto* b = static_cast<BlockOSBinding*>(c);
    return b && b->dma_pool && dma_pool_sync(b->dma_pool, buffer, offset, length, direction);
}
bool bind_irq_register(void* c, IrqHandler handler, void* handler_context, IrqRegistration* out)
{
    auto* b = static_cast<BlockOSBinding*>(c);
    return b && b->kernel.irq_register &&
        b->kernel.irq_register(b->kernel.context, handler, handler_context, out);
}
void bind_irq_unregister(void* c, IrqRegistration* registration)
{
    auto* b = static_cast<BlockOSBinding*>(c);
    if (b && b->kernel.irq_unregister)
        b->kernel.irq_unregister(b->kernel.context, registration);
}
uintptr_t bind_irq_save(void*)
{
    uintptr_t flags;
    asm volatile("pushfq; popq %0; cli" : "=r"(flags) :: "memory");
    return flags;
}
void bind_irq_restore(void*, uintptr_t flags)
{
    asm volatile("pushq %0; popfq" :: "r"(flags) : "memory", "cc");
}
uint64_t bind_monotonic_ns(void* c)
{
    const auto* b = static_cast<const BlockOSBinding*>(c);
    if (!b || b->tsc_frequency_hz == 0) return 0;
    const uint64_t ticks = read_tsc();
    const uint64_t whole = ticks / b->tsc_frequency_hz;
    const uint64_t rem = ticks % b->tsc_frequency_hz;
    if (whole > UINT64_MAX / 1000000000ull) return UINT64_MAX;
    return whole * 1000000000ull + (rem * 1000000000ull) / b->tsc_frequency_hz;
}
void bind_delay_us(void* c, uint32_t us)
{
    const auto* b = static_cast<const BlockOSBinding*>(c);
    if (!b || b->tsc_frequency_hz == 0 || us == 0) return;
    const uint64_t start = read_tsc();
    const uint64_t ticks_per_us = b->tsc_frequency_hz / 1000000ull;
    const uint64_t wanted = ticks_per_us * static_cast<uint64_t>(us);
    if (wanted == 0) return;
    while (read_tsc() - start < wanted) asm volatile("pause" ::: "memory");
}
bool bind_gsp_boot(void* c, const PciAddress* p, const MmioMapping* m,
                   const DmaBuffer* fw, size_t size, DmaBuffer* scratch)
{
    auto* b = static_cast<BlockOSBinding*>(c);
    return b && b->kernel.gsp_boot &&
        b->kernel.gsp_boot(b->kernel.context, p, m, fw, size, scratch);
}
bool bind_gsp_service(void* c, const PciAddress* p, const MmioMapping* m, uint32_t budget)
{
    auto* b = static_cast<BlockOSBinding*>(c);
    return b && b->kernel.gsp_service &&
        b->kernel.gsp_service(b->kernel.context, p, m, budget);
}
}

Status make_blockos_platform_ops(BlockOSBinding* b, PlatformOps* out)
{
    if (!b || !out || !b->dma_pool || !b->dma_pool->initialized ||
        b->tsc_frequency_hz == 0 || b->tsc_frequency_hz > UINT64_MAX / 1000000000ull ||
        !b->kernel.map_mmio || !b->kernel.unmap_mmio ||
        !b->kernel.irq_register || !b->kernel.irq_unregister)
        return Status::InvalidArgument;
    *out = {};
    out->abi_version = OPS_ABI_VERSION;
    out->struct_size = sizeof(PlatformOps);
    out->context = b;
    out->map_mmio = bind_map;
    out->unmap_mmio = bind_unmap;
    out->dma_allocate = bind_dma_allocate;
    out->dma_release = bind_dma_release;
    out->dma_sync = bind_dma_sync;
    out->irq_register = bind_irq_register;
    out->irq_unregister = bind_irq_unregister;
    out->irq_save_disable = bind_irq_save;
    out->irq_restore = bind_irq_restore;
    out->monotonic_ns = bind_monotonic_ns;
    out->delay_us = bind_delay_us;
    out->gsp_boot = b->kernel.gsp_boot ? bind_gsp_boot : nullptr;
    out->gsp_service = b->kernel.gsp_service ? bind_gsp_service : nullptr;
    return Status::Ok;
}

} // namespace nvidia_port
