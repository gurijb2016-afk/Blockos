#ifndef BLOCKOS_ARPA_INET_H
#define BLOCKOS_ARPA_INET_H
#include <netinet/in.h>
char *inet_ntoa(struct in_addr in);
#endif

#ifndef BLOCKOS_ARPA_INET_X11_COMPAT
#define BLOCKOS_ARPA_INET_X11_COMPAT
#include <sys/socket.h>
int inet_pton(int af, const char *src, void *dst);
const char *inet_ntop(int af, const void *src, char *dst, unsigned int size);
#endif
