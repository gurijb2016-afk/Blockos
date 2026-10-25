#pragma once
#include <stddef.h>
#include <stdint.h>
typedef long ssize_t;

typedef unsigned int socklen_t;
typedef uint16_t sa_family_t;

struct sockaddr { sa_family_t sa_family; char sa_data[14]; };
struct sockaddr_storage { sa_family_t ss_family; char __pad[126]; } __attribute__((aligned(8)));
struct sockaddr_un { sa_family_t sun_family; char sun_path[108]; };
#include <sys/uio.h>
struct msghdr {
    void* msg_name;
    socklen_t msg_namelen;
    struct iovec* msg_iov;
    size_t msg_iovlen;
    void* msg_control;
    size_t msg_controllen;
    int msg_flags;
};

#define AF_UNIX 1
#define AF_LOCAL AF_UNIX
#define AF_INET 2
#define AF_INET6 10
#define IPPROTO_IP 0
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17
struct in_addr { uint32_t s_addr; };
struct sockaddr_in { sa_family_t sin_family; uint16_t sin_port; struct in_addr sin_addr; uint8_t sin_zero[8]; };
struct in6_addr { uint8_t s6_addr[16]; };
struct sockaddr_in6 { sa_family_t sin6_family; uint16_t sin6_port; uint32_t sin6_flowinfo; struct in6_addr sin6_addr; uint32_t sin6_scope_id; };
#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#define SOCK_CLOEXEC 0x80000
#define SOCK_NONBLOCK 0x800
#define SHUT_RD 0
#define SHUT_WR 1
#define SHUT_RDWR 2
#define SOL_SOCKET 1
#define SO_ERROR 4

int socket(int domain, int type, int protocol);
int socketpair(int domain, int type, int protocol, int sv[2]);
int bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen);
int listen(int sockfd, int backlog);
int accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen);
int accept4(int sockfd, struct sockaddr* addr, socklen_t* addrlen, int flags);
int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen);
ssize_t send(int sockfd, const void* buf, size_t len, int flags);
ssize_t recv(int sockfd, void* buf, size_t len, int flags);
ssize_t sendto(int sockfd, const void* buf, size_t len, int flags, const struct sockaddr* dest, socklen_t addrlen);
ssize_t recvfrom(int sockfd, void* buf, size_t len, int flags, struct sockaddr* src, socklen_t* addrlen);
ssize_t sendmsg(int sockfd, const struct msghdr* msg, int flags);
ssize_t recvmsg(int sockfd, struct msghdr* msg, int flags);
int shutdown(int sockfd, int how);
int setsockopt(int sockfd,int level,int optname,const void* optval,socklen_t optlen);
int getsockopt(int sockfd,int level,int optname,void* optval,socklen_t* optlen);
#ifndef AF_UNSPEC
#define AF_UNSPEC 0
#endif
int getsockname(int fd, struct sockaddr *addr, unsigned int *len);
int getpeername(int fd, struct sockaddr *addr, unsigned int *len);

/* ancillary data (fd passing) and common socket options: declarations for
 * portable code such as libxcb; the kernel ignores the ancillary payload. */
struct cmsghdr { size_t cmsg_len; int cmsg_level; int cmsg_type; };
#define SCM_RIGHTS 1
#define SO_SNDBUF 7
#define SO_RCVBUF 8
#define SO_KEEPALIVE 9
#define MSG_TRUNC 0x20
#define MSG_CTRUNC 0x08
#define MSG_PEEK 0x02
#define MSG_DONTWAIT 0x40
#define MSG_NOSIGNAL 0x4000
#define __CMSG_ALIGN(n) (((n) + sizeof(size_t) - 1) & ~(sizeof(size_t) - 1))
#define CMSG_ALIGN(n) __CMSG_ALIGN(n)
#define CMSG_LEN(l) (CMSG_ALIGN(sizeof(struct cmsghdr)) + (l))
#define CMSG_SPACE(l) (CMSG_ALIGN(sizeof(struct cmsghdr)) + CMSG_ALIGN(l))
#define CMSG_DATA(c) ((unsigned char *)(c) + CMSG_ALIGN(sizeof(struct cmsghdr)))
#define CMSG_FIRSTHDR(m) \
    ((m)->msg_controllen >= sizeof(struct cmsghdr) ? (struct cmsghdr *)(m)->msg_control : (struct cmsghdr *)0)
#define CMSG_NXTHDR(m, c) \
    (((c)->cmsg_len < sizeof(struct cmsghdr)) ? (struct cmsghdr *)0 : \
    (((unsigned char *)(c) + CMSG_ALIGN((c)->cmsg_len) + sizeof(struct cmsghdr) > \
      (unsigned char *)(m)->msg_control + (m)->msg_controllen) ? (struct cmsghdr *)0 : \
     (struct cmsghdr *)((unsigned char *)(c) + CMSG_ALIGN((c)->cmsg_len))))
