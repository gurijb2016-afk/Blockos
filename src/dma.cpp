#include "dma.hpp"
#include "allocator.hpp"
#include <stddef.h>

static inline size_t align_up(size_t v, size_t a) { return (v + (a-1)) & ~(a-1); }

void* dma::alloc(size_t size, size_t align) {
    // use allocator::alloc which returns memory from the heapbuf reserved earlier
    return allocator::alloc( (size_t)align_up(size, align), align );
}
