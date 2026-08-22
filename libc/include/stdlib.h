#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void* malloc(size_t size);
void* calloc(size_t count, size_t size);
void* realloc(void* ptr, size_t size);
void free(void* ptr);

int atoi(const char* s);
long atol(const char* s);

unsigned long strtoul(const char* s, char** endptr, int base);

int abs(int x);
long labs(long x);

void srand(unsigned int seed);
int rand(void);

void qsort(
    void* base,
    size_t count,
    size_t size,
    int (*compare)(const void*, const void*)
);

[[noreturn]] void exit(int status);

#ifdef __cplusplus
}
#endif
