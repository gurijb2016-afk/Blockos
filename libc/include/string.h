#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void* memcpy(
    void* destination,
    const void* source,
    size_t size
);

void* memmove(
    void* destination,
    const void* source,
    size_t size
);

void* memset(
    void* destination,
    int value,
    size_t size
);

int memcmp(
    const void* a,
    const void* b,
    size_t size
);

size_t strlen(const char* str);

int strcmp(
    const char* a,
    const char* b
);

int strncmp(
    const char* a,
    const char* b,
    size_t n
);

char* strcpy(
    char* destination,
    const char* source
);

char* strncpy(
    char* destination,
    const char* source,
    size_t n
);

char* strcat(
    char* destination,
    const char* source
);

char* strncat(
    char* destination,
    const char* source,
    size_t n
);

char* strchr(
    const char* str,
    int character
);

char* strrchr(
    const char* str,
    int character
);

char* strstr(
    const char* haystack,
    const char* needle
);

char* strdup(const char* str);

#ifdef __cplusplus
}
#endif
