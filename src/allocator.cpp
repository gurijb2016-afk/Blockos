#include "allocator.hpp"
#include <stdint.h>
#include <stddef.h>

static uint8_t* heap_base = nullptr;
static size_t heap_size = 0;
static size_t heap_off = 0;

namespace allocator {
    void init(void* base, size_t size) {
        heap_base = (uint8_t*)base;
        heap_size = size;
        heap_off = 0;
    }

    static inline size_t align_up(size_t v, size_t a) { return (v + (a-1)) & ~(a-1); }

    void* alloc(size_t size, size_t align) {
        if (!heap_base) return nullptr;
        size_t off = align_up(heap_off, align);
        if (off + size > heap_size) return nullptr;
        void* ptr = heap_base + off;
        heap_off = off + size;
        return ptr;
    }

    void reset() {
        heap_off = 0;
    }
}

// global new/delete
void* operator new(std::size_t size) {
    return allocator::alloc(size, 8);
}
void operator delete(void* ptr) noexcept {
    (void)ptr; // no-op
}
void* operator new[](std::size_t size) {
    return allocator::alloc(size, 8);
}
void operator delete[](void* ptr) noexcept {
    (void)ptr;
}
