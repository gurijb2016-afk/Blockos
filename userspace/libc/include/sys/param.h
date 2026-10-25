#ifndef BLOCKOS_SYS_PARAM_H
#define BLOCKOS_SYS_PARAM_H
#include <limits.h>
#define MAXPATHLEN PATH_MAX
#define MAXHOSTNAMELEN HOST_NAME_MAX
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#define howmany(x,y) (((x)+((y)-1))/(y))
#define roundup(x,y) ((((x)+((y)-1))/(y))*(y))
#endif
