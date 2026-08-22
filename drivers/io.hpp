#pragma once

#include <stdint.h>

// x86 port I/O primitives.
//
// Every one of these is volatile with a memory clobber. Port access has side
// effects the compiler cannot see: a status register read is not redundant just
// because nothing changed in between, and at -O2 an unmarked read will be
// hoisted straight out of a polling loop.

namespace io
{

inline uint8_t inb(uint16_t port)
{
    uint8_t value;

    __asm__ __volatile__(
        "inb %1, %0"
        : "=a"(value)
        : "Nd"(port)
        : "memory");

    return value;
}

inline void outb(uint16_t port, uint8_t value)
{
    __asm__ __volatile__(
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
        : "memory");
}

inline uint16_t inw(uint16_t port)
{
    uint16_t value;

    __asm__ __volatile__(
        "inw %1, %0"
        : "=a"(value)
        : "Nd"(port)
        : "memory");

    return value;
}

inline void outw(uint16_t port, uint16_t value)
{
    __asm__ __volatile__(
        "outw %0, %1"
        :
        : "a"(value), "Nd"(port)
        : "memory");
}

// count is in words (2 bytes)
inline void insw(uint16_t port, void* buffer, uint32_t count)
{
    __asm__ __volatile__(
        "cld\n\t"
        "rep insw"
        : "+D"(buffer), "+c"(count)
        : "d"(port)
        : "memory");
}

inline void outsw(uint16_t port, const void* buffer, uint32_t count)
{
    __asm__ __volatile__(
        "cld\n\t"
        "rep outsw"
        : "+S"(buffer), "+c"(count)
        : "d"(port)
        : "memory");
}

} // namespace io
