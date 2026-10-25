#ifndef BLOCKOS_NETINET_IN_H
#define BLOCKOS_NETINET_IN_H
#include <stdint.h>
#include <sys/socket.h>
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17
#define INADDR_ANY ((uint32_t)0x00000000)
#define INADDR_LOOPBACK ((uint32_t)0x7f000001)
static inline uint16_t htons(uint16_t x) { return (uint16_t)((x << 8) | (x >> 8)); }
static inline uint16_t ntohs(uint16_t x) { return htons(x); }
static inline uint32_t htonl(uint32_t x) { return __builtin_bswap32(x); }
static inline uint32_t ntohl(uint32_t x) { return __builtin_bswap32(x); }
typedef uint32_t in_addr_t;
static inline int IN6_IS_ADDR_V4MAPPED(const struct in6_addr *a) {
    const uint8_t *p = a->s6_addr;
    for (int i = 0; i < 10; i++) if (p[i]) return 0;
    return p[10] == 0xff && p[11] == 0xff;
}
static inline int IN6_IS_ADDR_LOOPBACK(const struct in6_addr *a) {
    for (int i = 0; i < 15; i++) if (a->s6_addr[i]) return 0;
    return a->s6_addr[15] == 1;
}
#define INET6_ADDRSTRLEN 46
#endif
extern const struct in6_addr in6addr_any;
extern const struct in6_addr in6addr_loopback;
#ifndef INADDR_BROADCAST
#define INADDR_BROADCAST ((uint32_t)0xffffffff)
#endif
