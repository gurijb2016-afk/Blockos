#ifdef __cplusplus
extern "C" {
#endif
#pragma once
#include <sys/types.h>
#include <stddef.h>

void* malloc(size_t size);
void* calloc(size_t nmemb, size_t size);
void* realloc(void* ptr, size_t size);
void  free(void* ptr);
void* aligned_alloc(size_t alignment, size_t size);
int   posix_memalign(void** memptr, size_t alignment, size_t size);
unsigned long strtoul(const char* s, char** endp, int base);
long  strtol(const char* s, char** endp, int base);

int   atoi(const char* s);
long  atol(const char* s);

__attribute__((noreturn)) void abort(void);
__attribute__((noreturn)) void exit(int code);

extern char **environ;
char* getenv(const char* name);
int setenv(const char* name, const char* value, int overwrite);
int unsetenv(const char* name);
int system(const char *cmd);

int abs(int);
long labs(long);
int rand(void);
#ifndef RAND_MAX
#define RAND_MAX 2147483647
#endif
void srand(unsigned int);
int system(const char *cmd);
void qsort(void *base, size_t nmemb, size_t size, int (*cmp)(const void *, const void *));
double strtod(const char *s, char **end);
float strtof(const char *s, char **end);
long double strtold(const char *s, char **end);
long long llabs(long long v);
long long atoll(const char *s);
void _Exit(int status);
double atof(const char *s);
void srandom(unsigned int seed);
long random(void);

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#endif

/* X11 port compat (C locale only) */
#ifndef BLOCKOS_STDLIB_X11_COMPAT
#define BLOCKOS_STDLIB_X11_COMPAT
#define MB_CUR_MAX 1
void arc4random_buf(void *buf, size_t n);
int atexit(void (*fn)(void));
int mkstemp(char *tmpl);
int putenv(char *s);
void __blockos_run_atexit(void);
void *reallocarray(void *p, size_t nmemb, size_t size);
int mblen(const char *s, size_t n);
int mbtowc(wchar_t *pwc, const char *s, size_t n);
int wctomb(char *s, wchar_t wc);
size_t mbstowcs(wchar_t *dst, const char *src, size_t n);
#endif
void *bsearch(const void *key, const void *base, size_t nmemb, size_t size, int (*cmp)(const void *, const void *));

#include <alloca.h>

typedef struct { int quot; int rem; } div_t;
typedef struct { long quot; long rem; } ldiv_t;
typedef struct { long long quot; long long rem; } lldiv_t;
div_t div(int numer, int denom);
ldiv_t ldiv(long numer, long denom);
lldiv_t lldiv(long long numer, long long denom);

unsigned long long strtoull(const char*, char**, int);
long long strtoll(const char*, char**, int);
int clearenv(void);
char* realpath(const char*, char*);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif
char *mkdtemp(char *template);
#ifdef __cplusplus
}
#endif
