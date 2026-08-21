#include "../include/stdlib.h"
#include "../include/string.h"

#include <stddef.h>
#include <stdint.h>

#include "../../kernel/memory/allocator.hpp"

extern "C"
{

void* malloc(size_t size)
{
    if (size == 0)
        size = 1;

    return allocator::alloc(
        size,
        alignof(std::max_align_t));
}


void free(void* ptr)
{
    allocator::free(ptr);
}


void* calloc(size_t count, size_t size)
{
    if (count != 0 &&
        size > SIZE_MAX / count)
    {
        return nullptr;
    }

    size_t total = count * size;

    if (total == 0)
        total = 1;

    void* ptr = malloc(total);

    if (!ptr)
        return nullptr;

    memset(ptr, 0, total);

    return ptr;
}


void* realloc(void* ptr, size_t new_size)
{
    if (!ptr)
        return malloc(new_size);

    if (new_size == 0)
    {
        free(ptr);
        return nullptr;
    }

    size_t old_size =
        allocator::usable_size(ptr);

    if (old_size == 0)
        return nullptr;

    if (new_size <= old_size)
        return ptr;

    void* new_ptr =
        malloc(new_size);

    if (!new_ptr)
        return nullptr;

    memcpy(
        new_ptr,
        ptr,
        old_size);

    free(ptr);

    return new_ptr;
}


int atoi(const char* str)
{
    if (!str)
        return 0;

    while (*str == ' ' ||
           *str == '\t' ||
           *str == '\n' ||
           *str == '\r' ||
           *str == '\f' ||
           *str == '\v')
    {
        ++str;
    }

    int sign = 1;

    if (*str == '-')
    {
        sign = -1;
        ++str;
    }
    else if (*str == '+')
    {
        ++str;
    }

    int result = 0;

    while (*str >= '0' && *str <= '9')
    {
        result =
            result * 10 +
            (*str - '0');

        ++str;
    }

    return result * sign;
}


long atol(const char* str)
{
    if (!str)
        return 0;

    while (*str == ' ' ||
           *str == '\t' ||
           *str == '\n' ||
           *str == '\r' ||
           *str == '\f' ||
           *str == '\v')
    {
        ++str;
    }

    long sign = 1;

    if (*str == '-')
    {
        sign = -1;
        ++str;
    }
    else if (*str == '+')
    {
        ++str;
    }

    long result = 0;

    while (*str >= '0' && *str <= '9')
    {
        result =
            result * 10 +
            (*str - '0');

        ++str;
    }

    return result * sign;
}


int abs(int value)
{
    return value < 0 ? -value : value;
}


long labs(long value)
{
    return value < 0 ? -value : value;
}


/*
 * Simple deterministic PRNG.
 * Good enough for Doom-style non-cryptographic use.
 */
static uint32_t random_state = 1;


void srand(unsigned int seed)
{
    if (seed == 0)
        seed = 1;

    random_state = seed;
}


int rand(void)
{
    random_state =
        random_state * 1664525u +
        1013904223u;

    return static_cast<int>(
        (random_state >> 1) &
        0x7fffffff);
}


static void swap_bytes(
    unsigned char* a,
    unsigned char* b,
    size_t size)
{
    for (size_t i = 0; i < size; ++i)
    {
        unsigned char tmp = a[i];
        a[i] = b[i];
        b[i] = tmp;
    }
}


static void qsort_impl(
    unsigned char* base,
    size_t left,
    size_t right,
    size_t size,
    int (*compare)(
        const void*,
        const void*))
{
    if (left >= right)
        return;

    size_t i = left;
    size_t j = right;

    size_t pivot_index =
        left + (right - left) / 2;

    unsigned char* pivot =
        static_cast<unsigned char*>(
            malloc(size));

    if (!pivot)
        return;

    memcpy(
        pivot,
        base + pivot_index * size,
        size);

    while (i <= j)
    {
        while (
            compare(
                base + i * size,
                pivot) < 0)
        {
            ++i;

            if (i > right)
                break;
        }

        while (
            compare(
                base + j * size,
                pivot) > 0)
        {
            if (j == 0)
                break;

            --j;
        }

        if (i <= j)
        {
            swap_bytes(
                base + i * size,
                base + j * size,
                size);

            ++i;

            if (j > 0)
                --j;
            else
                break;
        }
    }

    free(pivot);

    if (left < j)
        qsort_impl(
            base,
            left,
            j,
            size,
            compare);

    if (i < right)
        qsort_impl(
            base,
            i,
            right,
            size,
            compare);
}


void qsort(
    void* base,
    size_t count,
    size_t size,
    int (*compare)(
        const void*,
        const void*))
{
    if (!base ||
        count < 2 ||
        size == 0 ||
        !compare)
    {
        return;
    }

    qsort_impl(
        static_cast<unsigned char*>(base),
        0,
        count - 1,
        size,
        compare);
}


[[noreturn]]
void exit(int status)
{
    /*
     * TODO:
     * Replace this with the BlockOS process
     * termination syscall when available.
     */

    (void)status;

    for (;;)
    {
#if defined(__x86_64__)
        asm volatile("cli; hlt");
#else
        for (;;)
            asm volatile("");
#endif
    }
}

}
