#include "../include/string.h"
#include "allocator.hpp"

#include <stddef.h>

extern "C"
{

void* memcpy(void* dst, const void* src, size_t n)
{
    unsigned char* d =
        static_cast<unsigned char*>(dst);

    const unsigned char* s =
        static_cast<const unsigned char*>(src);

    for (size_t i = 0; i < n; ++i)
        d[i] = s[i];

    return dst;
}


void* memmove(void* dst, const void* src, size_t n)
{
    unsigned char* d =
        static_cast<unsigned char*>(dst);

    const unsigned char* s =
        static_cast<const unsigned char*>(src);

    if (d == s || n == 0)
        return dst;

    if (d < s)
    {
        for (size_t i = 0; i < n; ++i)
            d[i] = s[i];
    }
    else
    {
        for (size_t i = n; i != 0; --i)
            d[i - 1] = s[i - 1];
    }

    return dst;
}


void* memset(void* dst, int c, size_t n)
{
    unsigned char* d =
        static_cast<unsigned char*>(dst);

    unsigned char value =
        static_cast<unsigned char>(c);

    for (size_t i = 0; i < n; ++i)
        d[i] = value;

    return dst;
}


int memcmp(const void* a, const void* b, size_t n)
{
    const unsigned char* x =
        static_cast<const unsigned char*>(a);

    const unsigned char* y =
        static_cast<const unsigned char*>(b);

    for (size_t i = 0; i < n; ++i)
    {
        if (x[i] < y[i])
            return -1;

        if (x[i] > y[i])
            return 1;
    }

    return 0;
}


size_t strlen(const char* s)
{
    if (!s)
        return 0;

    size_t n = 0;

    while (s[n] != '\0')
        ++n;

    return n;
}


int strcmp(const char* a, const char* b)
{
    while (*a && *a == *b)
    {
        ++a;
        ++b;
    }

    unsigned char ca =
        static_cast<unsigned char>(*a);

    unsigned char cb =
        static_cast<unsigned char>(*b);

    return (ca > cb) - (ca < cb);
}


int strncmp(const char* a, const char* b, size_t n)
{
    for (size_t i = 0; i < n; ++i)
    {
        unsigned char ca =
            static_cast<unsigned char>(a[i]);

        unsigned char cb =
            static_cast<unsigned char>(b[i]);

        if (ca != cb)
            return (ca > cb) - (ca < cb);

        if (ca == '\0')
            return 0;
    }

    return 0;
}


char* strcpy(char* dst, const char* src)
{
    char* result = dst;

    while ((*dst++ = *src++) != '\0')
        ;

    return result;
}


char* strncpy(char* dst, const char* src, size_t n)
{
    size_t i = 0;

    while (i < n && src[i])
    {
        dst[i] = src[i];
        ++i;
    }

    while (i < n)
    {
        dst[i] = '\0';
        ++i;
    }

    return dst;
}


char* strcat(char* dst, const char* src)
{
    char* result = dst;

    while (*dst)
        ++dst;

    while ((*dst++ = *src++) != '\0')
        ;

    return result;
}


char* strncat(char* dst, const char* src, size_t n)
{
    char* result = dst;

    while (*dst)
        ++dst;

    size_t i = 0;

    while (i < n && src[i])
    {
        dst[i] = src[i];
        ++i;
    }

    dst[i] = '\0';

    return result;
}


char* strchr(const char* s, int c)
{
    unsigned char wanted =
        static_cast<unsigned char>(c);

    while (*s)
    {
        if (static_cast<unsigned char>(*s) == wanted)
            return const_cast<char*>(s);

        ++s;
    }

    if (wanted == 0)
        return const_cast<char*>(s);

    return nullptr;
}


char* strrchr(const char* s, int c)
{
    const char* result = nullptr;

    unsigned char wanted =
        static_cast<unsigned char>(c);

    while (true)
    {
        if (static_cast<unsigned char>(*s) == wanted)
            result = s;

        if (*s == '\0')
            break;

        ++s;
    }

    return const_cast<char*>(result);
}


char* strstr(const char* haystack, const char* needle)
{
    if (!*needle)
        return const_cast<char*>(haystack);

    for (; *haystack; ++haystack)
    {
        const char* a = haystack;
        const char* b = needle;

        while (*a && *b && *a == *b)
        {
            ++a;
            ++b;
        }

        if (!*b)
            return const_cast<char*>(haystack);
    }

    return nullptr;
}


char* strdup(const char* s)
{
    if (!s)
        return nullptr;

    size_t length = strlen(s);

    char* result =
        static_cast<char*>(
            allocator::alloc(
                length + 1,
                alignof(char)));

    if (!result)
        return nullptr;

    memcpy(result, s, length + 1);

    return result;
}

}
