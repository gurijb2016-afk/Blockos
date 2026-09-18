#include "dlfcn.h"
#include "errno.h"

/*
 * BlockOS dynamic-loader API surface.
 * The loader must export these operations before GLib/GIO modules can use
 * dlopen/dlsym.  Until then, fail explicitly instead of returning a fake
 * handle that would crash a caller later.
 */
static __thread char g_dlerror[96];
static void seterr(const char* s) {
    int i = 0;
    if (!s) { g_dlerror[0] = 0; return; }
    for (; s[i] && i < 95; ++i) g_dlerror[i] = s[i];
    g_dlerror[i] = 0;
}
void* dlopen(const char* file, int mode) { (void)file; (void)mode; seterr("BlockOS ld.so: dlopen API unavailable"); errno = ENOSYS; return 0; }
void* dlsym(void* handle, const char* name) { (void)handle; (void)name; seterr("BlockOS ld.so: dlsym API unavailable"); errno = ENOSYS; return 0; }
int dlclose(void* handle) { (void)handle; seterr("BlockOS ld.so: dlclose API unavailable"); errno = ENOSYS; return -1; }
char* dlerror(void) { return g_dlerror[0] ? g_dlerror : 0; }
