#include "stdlib.h"
#include "unistd.h"
#include "string.h"
#include "sys/mman.h"
#include "errno.h"
#include "blockos_syscall.h"
#include <stdint.h>

#define ALIGNMENT 16u
#define MMAP_THRESHOLD (128u * 1024u)
#define BLOCK_MAGIC 0x424C4F434B4F5341ULL
#define ALIGN_MAGIC 0x414C49474E454431ULL
#define FLAG_FREE 0x1u
#define FLAG_MAPPED 0x2u

struct block {
    uint64_t magic;
    size_t size;
    size_t mapping_size;
    struct block* prev;
    struct block* next;
    uint32_t flags;
    uint32_t reserved;
};

struct align_header {
    uint64_t magic;
    void* raw;
    size_t size;
    size_t alignment;
};

static struct block* g_head;
static struct block* g_tail;
static volatile uint32_t g_heap_lock;
static char** g_environ;

extern void __blockos_set_environ(char **envp);

static void heap_lock(void) {
    while (__sync_lock_test_and_set(&g_heap_lock, 1u))
        __asm__ volatile("pause");
}

static void heap_unlock(void) {
    __sync_lock_release(&g_heap_lock);
}

static size_t align_up(size_t n, size_t a) {
    size_t m = a - 1;
    if (n > (size_t)-1 - m) return 0;
    return (n + m) & ~m;
}

static size_t page_up(size_t n) {
    return align_up(n, 4096u);
}

static int is_power_of_two(size_t n) {
    return n != 0 && (n & (n - 1)) == 0;
}

static struct block* payload_block(void* ptr) {
    if (!ptr) return 0;
    return (struct block*)((unsigned char*)ptr - sizeof(struct block));
}

static int valid_block(struct block* b) {
    return b && b->magic == BLOCK_MAGIC &&
           (b->flags == 0 || b->flags == FLAG_FREE || b->flags == FLAG_MAPPED);
}

static void unlink_block(struct block* b) {
    if (!b) return;
    if (b->prev) b->prev->next = b->next; else g_head = b->next;
    if (b->next) b->next->prev = b->prev; else g_tail = b->prev;
    b->prev = b->next = 0;
}

static void split_block(struct block* b, size_t wanted) {
    if (!b || b->flags != 0 || b->size < wanted) return;
    size_t rem = b->size - wanted;
    if (rem < sizeof(struct block) + ALIGNMENT) return;

    struct block* n = (struct block*)((unsigned char*)(b + 1) + wanted);
    n->magic = BLOCK_MAGIC;
    n->size = rem - sizeof(struct block);
    n->mapping_size = 0;
    n->prev = b;
    n->next = b->next;
    n->flags = FLAG_FREE;
    n->reserved = 0;
    if (n->next) n->next->prev = n; else g_tail = n;
    b->next = n;
    b->size = wanted;
}

static struct block* extend_heap(size_t size) {
    size_t total = sizeof(struct block) + size;
    void* p = sbrk((long)total);
    if (p == (void*)-1) return 0;

    struct block* b = (struct block*)p;
    b->magic = BLOCK_MAGIC;
    b->size = size;
    b->mapping_size = 0;
    b->prev = g_tail;
    b->next = 0;
    b->flags = 0;
    b->reserved = 0;
    if (g_tail) g_tail->next = b; else g_head = b;
    g_tail = b;
    return b;
}

static struct block* map_block(size_t size) {
    size_t total = page_up(sizeof(struct block) + size);
    if (!total) return 0;
    void* mem = mmap(0, total, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) return 0;

    struct block* b = (struct block*)mem;
    b->magic = BLOCK_MAGIC;
    b->size = total - sizeof(struct block);
    b->mapping_size = total;
    b->prev = b->next = 0;
    b->flags = FLAG_MAPPED;
    b->reserved = 0;
    return b;
}

void* malloc(size_t size) {
    if (size == 0) size = 1;
    size = align_up(size, ALIGNMENT);
    if (!size) { errno = ENOMEM; return 0; }

    heap_lock();

    struct block* b = 0;
    if (size < MMAP_THRESHOLD) {
        for (b = g_head; b; b = b->next) {
            if (b->flags == FLAG_FREE && b->size >= size) {
                b->flags = 0;
                split_block(b, size);
                heap_unlock();
                return (void*)(b + 1);
            }
        }
        b = extend_heap(size);
    } else {
        b = map_block(size);
    }

    heap_unlock();
    if (!b) { errno = ENOMEM; return 0; }
    return (void*)(b + 1);
}

static struct align_header* aligned_header(void* ptr) {
    if (!ptr) return 0;
    struct align_header* h =
        (struct align_header*)((unsigned char*)ptr - sizeof(struct align_header));
    return h->magic == ALIGN_MAGIC ? h : 0;
}

void free(void* ptr) {
    if (!ptr) return;

    struct align_header* ah = aligned_header(ptr);
    if (ah) {
        void* raw = ah->raw;
        ah->magic = 0;
        free(raw);
        return;
    }

    struct block* b = payload_block(ptr);
    if (!valid_block(b)) return;

    if (b->flags == FLAG_MAPPED) {
        size_t len = b->mapping_size;
        b->magic = 0;
        if (len) (void)munmap((void*)b, len);
        return;
    }

    heap_lock();
    b->flags = FLAG_FREE;

    if (b->next && b->next->flags == FLAG_FREE) {
        struct block* n = b->next;
        b->size += sizeof(struct block) + n->size;
        b->next = n->next;
        if (b->next) b->next->prev = b; else g_tail = b;
        n->magic = 0;
    }

    if (b->prev && b->prev->flags == FLAG_FREE) {
        struct block* p = b->prev;
        p->size += sizeof(struct block) + b->size;
        p->next = b->next;
        if (p->next) p->next->prev = p; else g_tail = p;
        b->magic = 0;
        b = p;
    }

    /* Return a completely free top chunk to the kernel when possible. */
    if (b == g_tail && b->flags == FLAG_FREE) {
        void* cur = sbrk(0);
        unsigned char* end = (unsigned char*)(b + 1) + b->size;
        if (cur == end) {
            size_t total = sizeof(struct block) + b->size;
            if (total <= 0x7fffffffL && sbrk(-(long)total) != (void*)-1) {
                if (b->prev) b->prev->next = 0; else g_head = 0;
                g_tail = b->prev;
                b->magic = 0;
            }
        }
    }

    heap_unlock();
}

void* calloc(size_t nmemb, size_t size) {
    if (nmemb && size > (size_t)-1 / nmemb) {
        errno = ENOMEM;
        return 0;
    }
    size_t total = nmemb * size;
    void* p = malloc(total ? total : 1);
    if (p) memset(p, 0, total);
    return p;
}

void* realloc(void* ptr, size_t size) {
    if (!ptr) return malloc(size);
    if (size == 0) { free(ptr); return 0; }

    struct align_header* ah = aligned_header(ptr);
    if (ah) {
        if (ah->size >= size) return ptr;
        void* np = aligned_alloc(ah->alignment, size);
        if (!np) return 0;
        memcpy(np, ptr, ah->size);
        free(ptr);
        return np;
    }

    struct block* b = payload_block(ptr);
    if (!valid_block(b) || b->flags == FLAG_FREE) {
        errno = EINVAL;
        return 0;
    }

    size_t wanted = align_up(size, ALIGNMENT);
    if (!wanted) { errno = ENOMEM; return 0; }
    if (b->size >= wanted) {
        if (!(b->flags & FLAG_MAPPED)) split_block(b, wanted);
        return ptr;
    }

    heap_lock();
    if (!(b->flags & FLAG_MAPPED) && b->next && b->next->flags == FLAG_FREE &&
        b->size + sizeof(struct block) + b->next->size >= wanted) {
        struct block* n = b->next;
        b->size += sizeof(struct block) + n->size;
        b->next = n->next;
        if (b->next) b->next->prev = b; else g_tail = b;
        n->magic = 0;
        split_block(b, wanted);
        heap_unlock();
        return ptr;
    }
    heap_unlock();

    void* np = malloc(size);
    if (!np) return 0;
    size_t copy = b->size < size ? b->size : size;
    memcpy(np, ptr, copy);
    free(ptr);
    return np;
}

void* aligned_alloc(size_t alignment, size_t size) {
    if (!is_power_of_two(alignment) || alignment < sizeof(void*)) {
        errno = EINVAL;
        return 0;
    }
    if (size && size % alignment != 0) {
        errno = EINVAL;
        return 0;
    }

    if (size == 0) size = alignment;
    if (size > (size_t)-1 - alignment - sizeof(struct align_header)) {
        errno = ENOMEM;
        return 0;
    }

    size_t extra = alignment + sizeof(struct align_header);
    void* raw = malloc(size + extra);
    if (!raw) return 0;

    uintptr_t start = (uintptr_t)raw + sizeof(struct align_header);
    uintptr_t aligned = (start + alignment - 1) & ~(uintptr_t)(alignment - 1);
    struct align_header* h =
        (struct align_header*)(aligned - sizeof(struct align_header));
    h->magic = ALIGN_MAGIC;
    h->raw = raw;
    h->size = size;
    h->alignment = alignment;
    return (void*)aligned;
}

int posix_memalign(void** memptr, size_t alignment, size_t size) {
    if (!memptr || alignment < sizeof(void*) || !is_power_of_two(alignment))
        return EINVAL;
    void* p = aligned_alloc(alignment, align_up(size ? size : alignment, alignment));
    if (!p) return errno ? errno : ENOMEM;
    *memptr = p;
    return 0;
}

int atoi(const char* s) {
    return (int)strtol(s, 0, 10);
}

long atol(const char* s) {
    return strtol(s, 0, 10);
}

long strtol(const char* s, char** endp, int base) {
    if (!s || base < 2 || base > 36) {
        if (endp) *endp = (char*)s;
        errno = EINVAL;
        return 0;
    }
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') ++s;
    int neg = 0;
    if (*s == '+' || *s == '-') { neg = *s == '-'; ++s; }
    long v = 0;
    const char* start = s;
    for (;;) {
        int d;
        if (*s >= '0' && *s <= '9') d = *s - '0';
        else if (*s >= 'a' && *s <= 'z') d = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'Z') d = *s - 'A' + 10;
        else break;
        if (d >= base) break;
        if (v > ((long)((~(unsigned long)0) >> 1) - d) / base) {
            errno = ERANGE;
            v = (long)((~(unsigned long)0) >> 1);
            break;
        }
        v = v * base + d;
        ++s;
    }
    if (endp) *endp = (char*)(s == start ? start : s);
    return neg ? -v : v;
}

void abort(void) {
    (void)__blockos_syscall(45, 134, 0, 0, 0, 0, 0);
    for (;;) __asm__ volatile("hlt");
}

void exit(int code) {
    _exit(code);
}

char **environ = 0;

char* getenv(const char* name) {
    if (!name || !*name || !environ) return 0;
    size_t n = strlen(name);
    for (char** e = environ; *e; ++e)
        if (strncmp(*e, name, n) == 0 && (*e)[n] == '=') return *e + n + 1;
    return 0;
}

int setenv(const char* name, const char* value, int overwrite) {
    (void)name; (void)value; (void)overwrite;
    /* The initial process environment is exposed read-only for now. */
    return 0;
}

int unsetenv(const char* name) { (void)name; return 0; }

void __blockos_set_environ(char **envp) { environ = envp; g_environ = envp; }
