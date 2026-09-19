#ifndef BLOCKOS_PTHREAD_COMPAT_H
#define BLOCKOS_PTHREAD_COMPAT_H
#include <pthread.h>

/*
 * Keep this tiny: GLib needs the normal POSIX pthread surface. BlockOS libc
 * supplies the actual implementation; these helpers only isolate places
 * where the port needs a direct hook.
 */
static inline int blockos_pthread_atfork(void (*prepare)(void),
                                          void (*parent)(void),
                                          void (*child)(void)) {
    return pthread_atfork(prepare, parent, child);
}
#endif
