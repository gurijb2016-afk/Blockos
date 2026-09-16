#include "sys/stat.h"
#include "blockos_syscall.h"
#include "errno.h"
#include "string.h"

int fstat(int fd, struct stat* buf) {
    memset(buf, 0, sizeof(*buf));
    long r = __blockos_syscall(__SYS_fstat, fd, (long)buf, 0, 0, 0, 0);
    if (r < 0) { errno = (int)-r; return -1; }
    return 0;
}
