#pragma once
#include <cstddef>

namespace allocator
{

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
