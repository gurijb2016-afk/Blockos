#include "drivers/nvidia_dma_pool.hpp"

namespace nvidia_port {
namespace {
constexpr uint64_t U64_MAX_VALUE = ~static_cast<uint64_t>(0);

bool is_power_of_two(size_t x) { return x && !(x & (x - 1)); }

void lock_pool(DmaPool* p)
{
    while (__atomic_exchange_n(&p->lock, 1u, __ATOMIC_ACQUIRE) != 0u)
        asm volatile("pause" ::: "memory");
}
void unlock_pool(DmaPool* p)
{
    __atomic_store_n(&p->lock, 0u, __ATOMIC_RELEASE);
}
bool bit_is_set(const DmaPool* p, size_t page)
{
    return (p->bitmap[page >> 3] & static_cast<uint8_t>(1u << (page & 7))) != 0;
}
void set_bit(DmaPool* p, size_t page, bool set)
{
    const uint8_t mask = static_cast<uint8_t>(1u << (page & 7));
    if (set) p->bitmap[page >> 3] |= mask;
    else p->bitmap[page >> 3] &= static_cast<uint8_t>(~mask);
}
void set_pages(DmaPool* p, size_t first, size_t count, bool set)
{
    for (size_t i = 0; i < count; ++i) set_bit(p, first + i, set);
}
bool run_free(const DmaPool* p, size_t first, size_t count)
{
    if (first > p->page_count || count > p->page_count - first) return false;
    for (size_t i = 0; i < count; ++i) if (bit_is_set(p, first + i)) return false;
    return true;
}
}

Status dma_pool_init(DmaPool* p, void* vbase, uint64_t pbase, size_t size,
                     uint8_t* bitmap, size_t bitmap_bytes, bool coherent)
{
    if (!p || !vbase || !bitmap || size < PAGE_SIZE ||
        (reinterpret_cast<uintptr_t>(vbase) & (PAGE_SIZE - 1)) != 0 ||
        (pbase & (PAGE_SIZE - 1)) != 0 || (size & (PAGE_SIZE - 1)) != 0)
        return Status::InvalidArgument;
    const size_t pages = size / PAGE_SIZE;
    const size_t need_bitmap = (pages + 7) / 8;
    if (bitmap_bytes < need_bitmap) return Status::InvalidArgument;
    if (size - 1 > U64_MAX_VALUE - pbase) return Status::InvalidArgument;

    *p = {};
    p->virtual_base = static_cast<uint8_t*>(vbase);
    p->physical_base = pbase;
    p->size = size;
    p->page_count = pages;
    p->bitmap = bitmap;
    p->bitmap_bytes = bitmap_bytes;
    p->coherent = coherent;
    p->initialized = true;
    p->lock = 0;
    for (size_t i = 0; i < bitmap_bytes; ++i) bitmap[i] = 0;
    for (size_t i = 0; i < DMA_POOL_MAX_ALLOCS; ++i) p->allocations[i] = {};
    return Status::Ok;
}

bool dma_pool_allocate(void* context, size_t size, size_t alignment,
                       uint64_t max_addr, DmaBuffer* out)
{
    auto* p = static_cast<DmaPool*>(context);
    if (!p || !p->initialized || !out || size == 0 ||
        !is_power_of_two(alignment) || alignment < sizeof(void*)) return false;
    *out = {};
    if (size > p->size || size > static_cast<size_t>(-1) - (PAGE_SIZE - 1)) return false;
    const size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    if (pages == 0 || pages > p->page_count) return false;
    uint64_t requested_end = 0;
    if (static_cast<uint64_t>(size - 1) > U64_MAX_VALUE - p->physical_base) return false;

    lock_pool(p);
    size_t record_index = DMA_POOL_MAX_ALLOCS;
    for (size_t i = 0; i < DMA_POOL_MAX_ALLOCS; ++i) {
        if (!p->allocations[i].used) { record_index = i; break; }
    }
    if (record_index == DMA_POOL_MAX_ALLOCS) { unlock_pool(p); return false; }

    for (size_t first = 0; first <= p->page_count - pages; ++first) {
        const uint64_t phys = p->physical_base + static_cast<uint64_t>(first) * PAGE_SIZE;
        const uintptr_t virt = reinterpret_cast<uintptr_t>(p->virtual_base + first * PAGE_SIZE);
        if ((phys & static_cast<uint64_t>(alignment - 1)) != 0 ||
            (virt & static_cast<uintptr_t>(alignment - 1)) != 0) continue;
        if (size - 1 > U64_MAX_VALUE - phys) continue;
        requested_end = phys + static_cast<uint64_t>(size - 1);
        if (requested_end > max_addr || !run_free(p, first, pages)) continue;
        set_pages(p, first, pages, true);
        p->allocations[record_index] = {first, pages, true};
        out->virtual_address = p->virtual_base + first * PAGE_SIZE;
        out->device_address = phys;
        out->size = size;
        out->alignment = alignment;
        out->cookie = &p->allocations[record_index];
        unlock_pool(p);
        return true;
    }
    unlock_pool(p);
    return false;
}

void dma_pool_release(void* context, DmaBuffer* b)
{
    auto* p = static_cast<DmaPool*>(context);
    if (!p || !p->initialized || !b || !b->virtual_address || !b->cookie) return;
    lock_pool(p);
    DmaPoolAllocation* rec = static_cast<DmaPoolAllocation*>(b->cookie);
    bool owned = false;
    for (size_t i = 0; i < DMA_POOL_MAX_ALLOCS; ++i)
        if (rec == &p->allocations[i]) { owned = true; break; }
    bool released = false;
    if (owned && rec->used && rec->page_count != 0 &&
        rec->first_page <= p->page_count &&
        rec->page_count <= p->page_count - rec->first_page &&
        b->virtual_address == p->virtual_base + rec->first_page * PAGE_SIZE &&
        b->device_address == p->physical_base + rec->first_page * PAGE_SIZE) {
        set_pages(p, rec->first_page, rec->page_count, false);
        *rec = {};
        released = true;
    }
    unlock_pool(p);
    if (released) *b = {};
}

bool dma_pool_sync(void* context, DmaBuffer* b, size_t offset, size_t length,
                   DmaDirection direction)
{
    auto* p = static_cast<DmaPool*>(context);
    (void)direction;
    if (!p || !p->initialized || !b || !b->virtual_address ||
        offset > b->size || length > b->size - offset) return false;
    if (!p->coherent) return false; // Refuse to pretend cache maintenance exists.
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    return true;
}

size_t dma_pool_free_pages(const DmaPool* p)
{
    if (!p || !p->initialized) return 0;
    size_t free_pages = 0;
    DmaPool* mutable_pool = const_cast<DmaPool*>(p);
    lock_pool(mutable_pool);
    for (size_t i = 0; i < p->page_count; ++i) if (!bit_is_set(p, i)) ++free_pages;
    unlock_pool(mutable_pool);
    return free_pages;
}

} // namespace nvidia_port
