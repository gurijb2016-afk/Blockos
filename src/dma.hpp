#pragma once
#include <cstddef>

// Simple DMA allocation wrapper for virtio use. Uses the existing allocator::alloc which
// places memory in the heap region allocated before ExitBootServices. Drivers MUST ensure
// allocator::init was called with a DMA-able buffer before ExitBootServices.

namespace dma {
    void* alloc(size_t size, size_t align = 4096);
}
