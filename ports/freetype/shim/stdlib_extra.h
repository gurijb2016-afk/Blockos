#pragma once
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
void *bsearch(const void *key, const void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
#ifdef __cplusplus
}
#endif
