#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void* malloc(size_t size);
void* calloc(size_t count, size_t size);
void* realloc(void* ptr, size_t size);
void free(void* ptr);

void exit(int status);

int atoi(const char* str);
long atol(const char* str);

int abs(int value);
long labs(long value);

void srand(unsigned int seed);
int rand(void);

void qsort(
    void* base,
    size_t count,
    size_t size,
    int (*compare)(const void*, const void*)
);

#ifdef __cplusplus
}
#endif
