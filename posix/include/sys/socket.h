#pragma once
#include <sys/types.h>
#include <stddef.h>
typedef unsigned int socklen_t;
struct sockaddr { unsigned short sa_family; char sa_data[14]; };
struct sockaddr_storage { unsigned short ss_family; char __data[126]; };
#define AF_UNIX 1
#define AF_INET 2
#define AF_INET6 10
#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#define SOCK_RAW 3
#define SHUT_RD 0
#define SHUT_WR 1
#define SHUT_RDWR 2
#define SOL_SOCKET 1
#define SO_REUSEADDR 2
#define SO_ERROR 4
#ifdef __cplusplus
extern "C" {
int socket(int,int,int); int bind(int,const struct sockaddr*,socklen_t); int listen(int,int); int accept(int,struct sockaddr*,socklen_t*); int accept4(int,struct sockaddr*,socklen_t*,int);
int connect(int,const struct sockaddr*,socklen_t); ssize_t sendto(int,const void*,size_t,int,const struct sockaddr*,socklen_t); ssize_t recvfrom(int,void*,size_t,int,struct sockaddr*,socklen_t*);
int shutdown(int,int); int setsockopt(int,int,int,const void*,socklen_t); int getsockopt(int,int,int,void*,socklen_t*);
}
#endif
