#pragma once
#include <stddef.h>
#define FD_SETSIZE 1024
#define __NFDBITS (8 * sizeof(unsigned long))
typedef struct { unsigned long bits[(FD_SETSIZE + __NFDBITS - 1) / __NFDBITS]; } fd_set;
#define FD_ZERO(p) do { for (size_t __i=0; __i<sizeof((p)->bits)/sizeof((p)->bits[0]); ++__i) (p)->bits[__i]=0; } while (0)
#define FD_SET(fd,p) ((p)->bits[(fd)/__NFDBITS] |= (1UL << ((fd)%__NFDBITS)))
#define FD_CLR(fd,p) ((p)->bits[(fd)/__NFDBITS] &= ~(1UL << ((fd)%__NFDBITS)))
#define FD_ISSET(fd,p) (((p)->bits[(fd)/__NFDBITS] >> ((fd)%__NFDBITS)) & 1UL)
