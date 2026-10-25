#pragma once
#include <stdint.h>
#include <sys/types.h>
struct winsize { uint16_t ws_row,ws_col,ws_xpixel,ws_ypixel; };
#define TIOCGWINSZ 0x424F5301UL
#define TIOCSWINSZ 0x424F5302UL
#define TIOCGETA 0x424F5303UL
#define TIOCSETA 0x424F5304UL
#define TIOCGPGRP 0x424F5305UL
#define TIOCSPGRP 0x424F5306UL
#define TCFLSH 0x424F5307UL
#define FIONBIO 0x424F5308UL
#define FIONREAD 0x424F5309UL
int ioctl(int fd,unsigned long request,...);
