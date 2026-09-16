#include "stdlib.h"
#include "unistd.h"
#include "string.h"

/*
 * Deliberately simple first-fit allocator: a singly-linked list of blocks,
 * each grown via sbrk() and never returned to the kernel (no munmap/brk
 * shrink path is implemented - see mman.c's note on munmap). free() just
 * marks a block reusable; there is no coalescing of adjacent free blocks.
 * Fine for a bootstrap program; a long-running process doing lots of
 * different-sized alloc/free churn will fragment. Revisit with a real
 * allocator (e.g. binning by size class) once something real needs it.
 *
 * NOT thread-safe - fine for now since BlockOS has no real threads yet
 * (see pthread.h).
 */
struct block { size_t size; struct block* next; int free; };
#define ALIGN 16u
static struct block* g_head = 0;

static size_t align_up(size_t n) { return (n + (ALIGN - 1)) & ~(size_t)(ALIGN - 1); }

static struct block* extend(size_t size) {
    size_t total = sizeof(struct block) + size;
    void* p = sbrk((long)total);
    if (p == (void*)-1) return 0;
    struct block* b = (struct block*)p;
    b->size = size;
    b->next = 0;
    b->free = 0;
    return b;
}

void* malloc(size_t size) {
    if (size == 0) return 0;
    size = align_up(size);

    struct block* prev = 0;
    struct block* b = g_head;
    while (b) {
        if (b->free && b->size >= size) { b->free = 0; return (void*)(b + 1); }
        prev = b;
        b = b->next;
    }

    struct block* nb = extend(size);
    if (!nb) return 0;
    if (prev) prev->next = nb; else g_head = nb;
    return (void*)(nb + 1);
}

void free(void* ptr) {
    if (!ptr) return;
    struct block* b = (struct block*)ptr - 1;
    b->free = 1;
}

void* calloc(size_t nmemb, size_t size) {
    /* No overflow check on nmemb*size - MVP; add one before relying on
     * this for untrusted sizes. */
    size_t total = nmemb * size;
    void* p = malloc(total);
    if (p) memset(p, 0, total);
    return p;
}

void* realloc(void* ptr, size_t size) {
    if (!ptr) return malloc(size);
    if (size == 0) { free(ptr); return 0; }
    struct block* b = (struct block*)ptr - 1;
    if (b->size >= size) return ptr;
    void* np = malloc(size);
    if (!np) return 0;
    memcpy(np, ptr, b->size);
    free(ptr);
    return np;
}

int atoi(const char* s) {
    int sign = 1;
    long v = 0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { sign = -1; s++; } else if (*s == '+') { s++; }
    while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); s++; }
    return (int)(sign * v);
}

long atol(const char* s) {
    long sign = 1, v = 0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { sign = -1; s++; } else if (*s == '+') { s++; }
    while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); s++; }
    return sign * v;
}

void abort(void) {
    _exit(134); /* conventional 128+SIGABRT exit code, even though BlockOS has no real signals yet */
}

void exit(int code) {
    /* No atexit()/destructor support in this MVP - add it if something
     * you're porting actually relies on cleanup handlers running. */
    _exit(code);
}

char* getenv(const char* name) {
    (void)name;
    return 0; /* no environment storage yet - see the note in stdlib.h */
}
