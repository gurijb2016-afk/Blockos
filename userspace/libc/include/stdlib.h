#pragma once
#include <stddef.h>

void* malloc(size_t size);
void* calloc(size_t nmemb, size_t size);
void* realloc(void* ptr, size_t size);
void  free(void* ptr);

int   atoi(const char* s);
long  atol(const char* s);

__attribute__((noreturn)) void abort(void);
__attribute__((noreturn)) void exit(int code);

/* No environment variable storage in this MVP - always returns NULL.
 * Fine for programs that treat a missing env var as "use the default". */
char* getenv(const char* name);
