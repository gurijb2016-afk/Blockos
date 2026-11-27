#include "termios.h"
#include "sys/ioctl.h"
#include "errno.h"
#include <string.h>

/* The kernel's termios ABI: same flag bits as Linux, but the struct has no c_line/speeds and
 * 20 control characters. TIOC_GET/SETTERMIOS are the kernel's ioctl numbers for it. */
#define BOS_TIOC_GETTERMIOS 0x424F5303UL
#define BOS_TIOC_SETTERMIOS 0x424F5304UL

struct bos_termios { uint32_t iflag, oflag, cflag, lflag; uint8_t cc[20]; };

int tcgetattr(int fd, struct termios *t) {
    struct bos_termios k;
    if (!t) { errno = EINVAL; return -1; }
    if (ioctl(fd, BOS_TIOC_GETTERMIOS, &k) < 0) return -1;
    memset(t, 0, sizeof *t);
    t->c_iflag = k.iflag; t->c_oflag = k.oflag; t->c_cflag = k.cflag; t->c_lflag = k.lflag;
    memcpy(t->c_cc, k.cc, NCCS);
    t->c_ispeed = t->c_ospeed = B38400;
    return 0;
}

int tcsetattr(int fd, int action, const struct termios *t) {
    struct bos_termios k;
    (void)action; /* the kernel applies changes at once; there is no output queue to drain */
    if (!t) { errno = EINVAL; return -1; }
    k.iflag = t->c_iflag; k.oflag = t->c_oflag; k.cflag = t->c_cflag; k.lflag = t->c_lflag;
    memset(k.cc, 0, sizeof k.cc);
    memcpy(k.cc, t->c_cc, NCCS);
    return ioctl(fd, BOS_TIOC_SETTERMIOS, &k) < 0 ? -1 : 0;
}

int tcflush(int fd, int queue) { (void)fd; (void)queue; return 0; }

speed_t cfgetispeed(const struct termios *t) { return t ? t->c_ispeed : 0; }
speed_t cfgetospeed(const struct termios *t) { return t ? t->c_ospeed : 0; }
int cfsetispeed(struct termios *t, speed_t s) { if (!t) return -1; t->c_ispeed = s; return 0; }
int cfsetospeed(struct termios *t, speed_t s) { if (!t) return -1; t->c_ospeed = s; return 0; }
