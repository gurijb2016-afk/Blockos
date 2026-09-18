#include "sys/arch_prctl.h"
#include "blockos_syscall.h"
#include "errno.h"

long arch_prctl(int code, unsigned long addr) {
    long r = __blockos_syscall(__SYS_arch_prctl, code, (long)addr, 0, 0, 0, 0);
    if (r < 0) { errno = (int)-r; return -1; }
    return r;
}
