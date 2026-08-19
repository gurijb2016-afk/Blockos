#include "allocator.hpp"

#include <stddef.h>
#include <stdint.h>

#include <cstddef>
#include <cstdint>


static uint8_t* heap_base = nullptr;
static size_t heap_size = 0;
static size_t heap_off = 0;

struct header
{
    size_t size;
    bool isFree;
};

namespace allocator
{

static inline bool is_power_of_2(size_t x)
{
    return (x != 0) && ((x & (x - 1)) == 0);
}

static inline size_t align_up(size_t v, size_t a)
{
    if (!is_power_of_2(a))
    { // If a is not a power of two, this is handled by
        // policy to be determined -> 0 is a placeholder here
        return 0; // Could throw an exception, or assert
    }
    return (v + (a - 1)) & ~(a - 1);
}

// Chosen to accomodate dma::alloc alignment, largest in the code base
static constexpr size_t ARENA_ALIGNMENT = 4 * 1024;

void init(void* base, size_t size)
{
    heap_base = (uint8_t*) base;
    uintptr_t addr = (uintptr_t) heap_base;
    uintptr_t aligned_addr =
        align_up(addr,
                 ARENA_ALIGNMENT); // Alternatively instead of forcing alignement
    // PANIC or throw an exception
    size_t lost = aligned_addr - addr;

    if (lost >= size)
    {
        heap_base = nullptr;
        return; // Not enough space to align, or size is too small
    }

    heap_base = (uint8_t*) aligned_addr;
    size -= lost; // Adjust size to accomodate lost bytes due to alignment
    heap_size = size;
    heap_off = 0;
}

static constexpr size_t GRANULARITY =
    sizeof(header); // This is the minimum alignment for any type

// Assumes commitment to payload + header convention
void* alloc(size_t size, size_t align)
{
    if (align < GRANULARITY)
        align = GRANULARITY; // Ensure alignment is at least the size of the header
    // to avoid header/general misalignment issues

    if (!heap_base || size > heap_size || !is_power_of_2(align))
        return nullptr;

    // Align the block length to the header alignment to ensure blocks are densely
    // packed
    size_t block_length = align_up(sizeof(header) + size, GRANULARITY);

    // Scan the heap for the first free block that is large enough to satisfy the
    // request and meets the alignment requirement
    size_t scan = 0;
    while (scan < heap_off)
    {
        header* h = (header*) (heap_base + scan);

        if (h->isFree)
        {
            uintptr_t payload = (uintptr_t) (heap_base + scan + sizeof(header));
            uintptr_t aligned_payload = align_up(payload, align);
            size_t difference = (size_t) (aligned_payload -
                                          payload); // this is lost to alignment slack

            if (h->size >= difference + block_length)
            {
                size_t lead_in_offset = difference + scan;
                size_t remainder =
                    h->size - difference -
                    block_length; // trailing free space after the allocated block

                if (difference > 0)
                {
                    if (difference < sizeof(header))
                    {
                        scan += h->size;
                        continue;
                    }
                    header* lead_in_block = (header*) (heap_base + scan);
                    lead_in_block->size = difference;
                    lead_in_block->isFree = true;
                }

                size_t new_block_size = block_length;
                if (remainder >= sizeof(header))
                {
                    // Reallocate the current block to the new size and mark it as free
                    header* trailing =
                        (header*) (heap_base + lead_in_offset + block_length);
                    trailing->size = remainder;
                    trailing->isFree = true;
                }
                else
                {
                    new_block_size += remainder;
                }

                // Create the new block at the lead-in offset and mark it as allocated
                header* h2 = (header*) (heap_base + lead_in_offset);
                h2->size = new_block_size;
                h2->isFree = false;
                return h2 +
                       1; // Return pointer to the payload, which is after the header
            }
        }
        if (h->size != 0)
            scan += h->size;
        else
            break; // should PANIC, corrupted heap
    }

    uintptr_t base = (uintptr_t) heap_base;
    uintptr_t payload =
        align_up((uintptr_t) (base + heap_off + sizeof(header)),
                 align); // Satisfy requested alignment for the payload
    size_t off = (size_t) (payload - base - sizeof(header));

    if (block_length > heap_size || off > heap_size - block_length)
        return nullptr;

    // If the offset is greater than the current heap offset, create a filler
    // block to fill the gap
    if (off > heap_off)
    {
        header* filler = (header*) (heap_base + heap_off);
        filler->size = off - heap_off;
        filler->isFree = true;
    }

    // Create a new block at the calculated offset and update the heap offset
    header* h = (header*) (heap_base + off);
    h->size = block_length;
    h->isFree = false;
    heap_off = off + block_length;
    return h + 1;
}

void free(void* ptr)
{
    if (!ptr || !heap_base) return;
    header* h = (header*) ptr - 1;
    size_t header_offset = (size_t) ((uint8_t*) h - heap_base);
    if (header_offset >= heap_off || header_offset % alignof(header) != 0 ||
        h->size == 0 || h->size > heap_off - header_offset || h->isFree)
        return; // should PANIC
    h->isFree = true;

    for (;;)
    { // absorb adjacent free blocks
        size_t total_off = header_offset + h->size;
        if (total_off >= heap_off)
            break;
        header* next = (header*) (heap_base + total_off);
        if (!next->isFree)
            break;
        h->size += next->size;
    }

    // Find the previous block and check if it is free
    size_t prev_off = header_offset;
    size_t scan = 0;
    while (scan < header_offset)
    {
        header* b = (header*) (heap_base + scan);
        if (b->size == 0 || scan + b->size > header_offset)
            break; // corrupt chain or pointer is not on a boundary
        prev_off = scan;
        scan += b->size;
    }

    // prev_off is now the block before the block being freed if scan == header_offset
    if (scan == header_offset && prev_off != header_offset)
    {
        header* prev = (header*) (heap_base + prev_off);
        if (prev->isFree)
        {
            prev->size += h->size;
            h = prev;
            header_offset = prev_off;
        }
    }

    if (header_offset + h->size == heap_off)
        heap_off = header_offset;
}

void reset()
{
    heap_off = 0;
}
} // namespace allocator

// global new/delete
void* operator new(std::size_t size)
{
    return allocator::alloc(
        size, alignof(std::max_align_t)); // Default alignment needs to support 16
    // - aligned types, so use max_align_t
}
void operator delete(void* ptr) noexcept
{
    allocator::free(ptr);
}
void operator delete(void* ptr, std::size_t) noexcept
{
    allocator::free(ptr);
}
void* operator new[](std::size_t size)
{
    return allocator::alloc(
        size, alignof(std::max_align_t)); // Default alignment needs to support
    // 16-aligned types, so use max_align_t
}
void operator delete[](void* ptr) noexcept
{
    allocator::free(ptr);
}
void operator delete[](void* ptr, std::size_t) noexcept
{
    allocator::free(ptr);
}
