#include "fcntl.h"
#include "blockos_syscall.h"
#include "errno.h"
#include <stdarg.h>

int open(const char* path, int flags, ...) {
    /* mode is accepted for source compatibility but the kernel doesn't
     * use it yet (see the note in fcntl.h). */
    va_list ap;
    va_start(ap, flags);
    int mode = 0;
    if (flags & O_CREAT) mode = va_arg(ap, int);
    va_end(ap);
    (void)mode;

    long r = __blockos_syscall(__SYS_openat, AT_FDCWD, (long)path, flags, mode, 0, 0);
    if (r < 0) { errno = (int)-r; return -1; }
    return (int)r;
}
