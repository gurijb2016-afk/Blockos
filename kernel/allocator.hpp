#pragma once
#include <cstddef>

namespace allocator
{

struct AllocRecord
{
    size_t total;
    size_t used;
    size_t offset; // heap_off
    size_t peak; // largest used since init() or reset()

    size_t alloc_count;
    size_t free_count;
    size_t alloc_failed;
    size_t free_rejected; // free() calls rejected by validation
    size_t reset_count;
};

// Snapshot of the running counters.
const AllocRecord& get_record();

void init(void* base, size_t size);
void* alloc(size_t size, size_t align = 8); // Should match GRANULARITY
void reset();
void free(void* ptr);
} // namespace allocator

// Global new/delete overrides
void* operator new(std::size_t size);
void operator delete(void* ptr) noexcept;
void operator delete(void* ptr, std::size_t) noexcept;
void* operator new[](std::size_t size);
void operator delete[](void* ptr) noexcept;
void operator delete[](void* ptr, std::size_t) noexcept;
