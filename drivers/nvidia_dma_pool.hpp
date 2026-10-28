#pragma once
#include "nvidia_port.hpp"

namespace nvidia_port {

constexpr size_t DMA_POOL_MAX_ALLOCS = 128;

struct DmaPoolAllocation {
    size_t first_page;
    size_t page_count;
    bool used;
};

/*
 * Fixed, contiguous DMA arena allocator. The caller must reserve a physically
 * contiguous RAM range before calling init(), and pass a kernel virtual mapping
 * of precisely that range. This allocator never assumes virtual==physical.
 *
 * The fixed bitmap and allocation table avoid heap allocation and work during
 * early kernel initialization. One DmaPool instance is for one reserved arena.
 */
struct DmaPool {
    uint8_t* virtual_base;
    uint64_t physical_base;
    size_t size;
    size_t page_count;
    uint8_t* bitmap;
    size_t bitmap_bytes;
    bool coherent;
    bool initialized;
    volatile uint32_t lock;
    DmaPoolAllocation allocations[DMA_POOL_MAX_ALLOCS];
};

Status dma_pool_init(DmaPool* pool, void* virtual_base, uint64_t physical_base,
                     size_t size, uint8_t* bitmap, size_t bitmap_bytes,
                     bool dma_coherent);
bool dma_pool_allocate(void* pool_context, size_t size, size_t alignment,
                       uint64_t max_device_address, DmaBuffer* out);
void dma_pool_release(void* pool_context, DmaBuffer* buffer);
bool dma_pool_sync(void* pool_context, DmaBuffer* buffer, size_t offset,
                   size_t length, DmaDirection direction);
size_t dma_pool_free_pages(const DmaPool* pool);

} // namespace nvidia_port
