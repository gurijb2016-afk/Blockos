#pragma once
#include <stddef.h>

void* malloc(size_t size);
void* calloc(size_t nmemb, size_t size);
void* realloc(void* ptr, size_t size);
void  free(void* ptr);
void* aligned_alloc(size_t alignment, size_t size);
int   posix_memalign(void** memptr, size_t alignment, size_t size);
long  strtol(const char* s, char** endp, int base);

int   atoi(const char* s);
long  atol(const char* s);

__attribute__((noreturn)) void abort(void);
__attribute__((noreturn)) void exit(int code);

extern char **environ;
char* getenv(const char* name);
int setenv(const char* name, const char* value, int overwrite);
int unsetenv(const char* name);
