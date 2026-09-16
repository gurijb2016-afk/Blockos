#include "string.h"

void* memcpy(void* dst, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dst;
}

void* memmove(void* dst, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    if (d == s || n == 0) return dst;
    if (d < s) {
        for (size_t i = 0; i < n; i++) d[i] = s[i];
    } else {
        for (size_t i = n; i != 0; i--) d[i-1] = s[i-1];
    }
    return dst;
}

void* memset(void* dst, int c, size_t n) {
    unsigned char* d = (unsigned char*)dst;
    unsigned char v = (unsigned char)c;
    for (size_t i = 0; i < n; i++) d[i] = v;
    return dst;
}

int memcmp(const void* a, const void* b, size_t n) {
    const unsigned char* pa = (const unsigned char*)a;
    const unsigned char* pb = (const unsigned char*)b;
    for (size_t i = 0; i < n; i++) {
        if (pa[i] != pb[i]) return (int)pa[i] - (int)pb[i];
    }
    return 0;
}

void* memchr(const void* s, int c, size_t n) {
    const unsigned char* p = (const unsigned char*)s;
    unsigned char v = (unsigned char)c;
    for (size_t i = 0; i < n; i++) if (p[i] == v) return (void*)(p + i);
    return 0;
}

size_t strlen(const char* s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

char* strcpy(char* dst, const char* src) {
    size_t i = 0;
    for (; src[i]; i++) dst[i] = src[i];
    dst[i] = 0;
    return dst;
}

char* strncpy(char* dst, const char* src, size_t n) {
    size_t i = 0;
    for (; i < n && src[i]; i++) dst[i] = src[i];
    for (; i < n; i++) dst[i] = 0;
    return dst;
}

char* strcat(char* dst, const char* src) {
    strcpy(dst + strlen(dst), src);
    return dst;
}

char* strncat(char* dst, const char* src, size_t n) {
    size_t dlen = strlen(dst);
    size_t i = 0;
    for (; i < n && src[i]; i++) dst[dlen + i] = src[i];
    dst[dlen + i] = 0;
    return dst;
}

int strcmp(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

int strncmp(const char* a, const char* b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i] || a[i] == 0) return (int)(unsigned char)a[i] - (int)(unsigned char)b[i];
    }
    return 0;
}

char* strchr(const char* s, int c) {
    while (*s) { if (*s == (char)c) return (char*)s; s++; }
    return (c == 0) ? (char*)s : 0;
}

char* strrchr(const char* s, int c) {
    const char* last = 0;
    while (*s) { if (*s == (char)c) last = s; s++; }
    if (c == 0) return (char*)s;
    return (char*)last;
}

char* strstr(const char* haystack, const char* needle) {
    if (!*needle) return (char*)haystack;
    for (; *haystack; haystack++) {
        const char* h = haystack;
        const char* n = needle;
        while (*h && *n && *h == *n) { h++; n++; }
        if (!*n) return (char*)haystack;
    }
    return 0;
}

extern void* malloc(size_t);

char* strdup(const char* s) {
    size_t n = strlen(s) + 1;
    char* p = (char*)malloc(n);
    if (!p) return 0;
    memcpy(p, s, n);
    return p;
}
