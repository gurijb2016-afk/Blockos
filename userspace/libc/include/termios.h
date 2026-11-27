#ifndef BLOCKOS_TERMIOS_H
#define BLOCKOS_TERMIOS_H
/* termios for BlockOS. Flag values are the Linux x86_64 ones; the kernel uses the same bits. */
#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>

typedef unsigned int tcflag_t;
typedef unsigned char cc_t;
typedef unsigned int speed_t;

#define NCCS 19

struct termios {
    tcflag_t c_iflag, c_oflag, c_cflag, c_lflag;
    cc_t c_line;
    cc_t c_cc[NCCS];
    speed_t c_ispeed, c_ospeed;
};

/* c_cc indices */
#define VINTR 0
#define VQUIT 1
#define VERASE 2
#define VKILL 3
#define VEOF 4
#define VTIME 5
#define VMIN 6
#define VSTART 8
#define VSTOP 9
#define VSUSP 10

/* c_iflag */
#define IGNBRK 0000001u
#define BRKINT 0000002u
#define IGNPAR 0000004u
#define PARMRK 0000010u
#define INPCK  0000020u
#define ISTRIP 0000040u
#define INLCR  0000100u
#define IGNCR  0000200u
#define ICRNL  0000400u
#define IXON   0002000u
#define IXOFF  0010000u

/* c_oflag */
#define OPOST 0000001u
#define ONLCR 0000004u

/* c_cflag */
#define CSIZE  0000060u
#define CS8    0000060u
#define CREAD  0000200u
#define PARENB 0000400u
#define PARODD 0001000u
#define HUPCL  0002000u
#define CLOCAL 0004000u

/* c_lflag */
#define ISIG   0000001u
#define ICANON 0000002u
#define ECHO   0000010u
#define ECHOE  0000020u
#define ECHOK  0000040u
#define ECHONL 0000100u
#define NOFLSH 0000200u
#define IEXTEN 0100000u

/* tcsetattr() actions and tcflush() queues */
#define TCSANOW 0
#define TCSADRAIN 1
#define TCSAFLUSH 2
#define TCIFLUSH 0
#define TCOFLUSH 1
#define TCIOFLUSH 2

/* speeds (Linux values) */
#define B0 0000000
#define B50 0000001
#define B75 0000002
#define B110 0000003
#define B134 0000004
#define B150 0000005
#define B200 0000006
#define B300 0000007
#define B600 0000010
#define B1200 0000011
#define B1800 0000012
#define B2400 0000013
#define B4800 0000014
#define B9600 0000015
#define B19200 0000016
#define B38400 0000017

int tcgetattr(int fd, struct termios *t);
int tcsetattr(int fd, int action, const struct termios *t);
int tcflush(int fd, int queue);
speed_t cfgetispeed(const struct termios *t);
speed_t cfgetospeed(const struct termios *t);
int cfsetispeed(struct termios *t, speed_t speed);
int cfsetospeed(struct termios *t, speed_t speed);

#ifdef __cplusplus
}
#endif
#endif
