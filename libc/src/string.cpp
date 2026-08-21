#include "../include/string.h"

#include <stddef.h>

#include "../../kernel/memory/allocator.hpp"

extern "C"
{

void* memcpy(
    void* destination,
    const void* source,
    size_t size)
{
    unsigned char* dst =
        static_cast<unsigned char*>(destination);

    const unsigned char* src =
        static_cast<const unsigned char*>(source);

    for (size_t i = 0; i < size; ++i)
        dst[i] = src[i];

    return destination;
}


void* memmove(
    void* destination,
    const void* source,
    size_t size)
{
    unsigned char* dst =
        static_cast<unsigned char*>(destination);

    const unsigned char* src =
        static_cast<const unsigned char*>(source);

    if (dst == src || size == 0)
        return destination;

    if (dst < src)
    {
        for (size_t i = 0; i < size; ++i)
            dst[i] = src[i];
    }
    else
    {
        for (size_t i = size; i > 0; --i)
            dst[i - 1] = src[i - 1];
    }

    return destination;
}


void* memset(
    void* destination,
    int value,
    size_t size)
{
    unsigned char* dst =
        static_cast<unsigned char*>(destination);

    unsigned char v =
        static_cast<unsigned char>(value);

    for (size_t i = 0; i < size; ++i)
        dst[i] = v;

    return destination;
}


int memcmp(
    const void* a,
    const void* b,
    size_t size)
{
    const unsigned char* x =
        static_cast<const unsigned char*>(a);

    const unsigned char* y =
        static_cast<const unsigned char*>(b);

    for (size_t i = 0; i < size; ++i)
    {
        if (x[i] < y[i])
            return -1;

        if (x[i] > y[i])
            return 1;
    }

    return 0;
}


size_t strlen(const char* str)
{
    if (!str)
        return 0;

    size_t length = 0;

    while (str[length] != '\0')
        ++length;

    return length;
}


int strcmp(
    const char* a,
    const char* b)
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

    if (ca < cb)
        return -1;

    if (ca > cb)
        return 1;

    return 0;
}


int strncmp(
    const char* a,
    const char* b,
    size_t n)
{
    if (n == 0)
        return 0;

    for (size_t i = 0; i < n; ++i)
    {
        unsigned char ca =
            static_cast<unsigned char>(a[i]);

        unsigned char cb =
            static_cast<unsigned char>(b[i]);

        if (ca < cb)
            return -1;

        if (ca > cb)
            return 1;

        if (ca == '\0')
            return 0;
    }

    return 0;
}


char* strcpy(
    char* destination,
    const char* source)
{
    char* result = destination;

    while ((*destination++ = *source++) != '\0')
        ;

    return result;
}


char* strncpy(
    char* destination,
    const char* source,
    size_t n)
{
    size_t i = 0;

    for (; i < n && source[i] != '\0'; ++i)
        destination[i] = source[i];

    for (; i < n; ++i)
        destination[i] = '\0';

    return destination;
}


char* strcat(
    char* destination,
    const char* source)
{
    char* result = destination;

    while (*destination)
        ++destination;

    while ((*destination++ = *source++) != '\0')
        ;

    return result;
}


char* strncat(
    char* destination,
    const char* source,
    size_t n)
{
    char* result = destination;

    while (*destination)
        ++destination;

    size_t i = 0;

    while (i < n && source[i] != '\0')
    {
        destination[i] = source[i];
        ++i;
    }

    destination[i] = '\0';

    return result;
}


char* strchr(
    const char* str,
    int character)
{
    unsigned char c =
        static_cast<unsigned char>(character);

    while (*str)
    {
        if (static_cast<unsigned char>(*str) == c)
            return const_cast<char*>(str);

        ++str;
    }

    if (c == '\0')
        return const_cast<char*>(str);

    return nullptr;
}


char* strrchr(
    const char* str,
    int character)
{
    const char* result = nullptr;

    unsigned char c =
        static_cast<unsigned char>(character);

    while (true)
    {
        if (static_cast<unsigned char>(*str) == c)
            result = str;

        if (*str == '\0')
            break;

        ++str;
    }

    return const_cast<char*>(result);
}


char* strstr(
    const char* haystack,
    const char* needle)
{
    if (!*needle)
        return const_cast<char*>(haystack);

    for (; *haystack; ++haystack)
    {
        const char* h = haystack;
        const char* n = needle;

        while (*h && *n && *h == *n)
        {
            ++h;
            ++n;
        }

        if (*n == '\0')
            return const_cast<char*>(haystack);
    }

    return nullptr;
}


char* strdup(const char* str)
{
    if (!str)
        return nullptr;

    size_t length = strlen(str);

    char* result =
        static_cast<char*>(
            allocator::alloc(
                length + 1,
                alignof(char)));

    if (!result)
        return nullptr;

    memcpy(result, str, length + 1);

    return result;
}

}
