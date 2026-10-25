#include "unistd.h"
#include "blockos_syscall.h"
#include "errno.h"
#include "signal.h"
#include "stdlib.h"
#include <string.h>

int tcsetpgrp(int fd,pid_t pgrp){(void)fd;(void)pgrp;return 0;}
unsigned alarm(unsigned seconds){(void)seconds;return 0;}
int killpg(pid_t pgrp,int sig){return kill(pgrp,sig);}

