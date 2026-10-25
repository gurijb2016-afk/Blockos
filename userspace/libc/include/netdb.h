#ifndef BLOCKOS_NETDB_H
#define BLOCKOS_NETDB_H
#include <sys/socket.h>
#include <netinet/in.h>
struct addrinfo { int ai_flags; int ai_family; int ai_socktype; int ai_protocol; size_t ai_addrlen; struct sockaddr *ai_addr; char *ai_canonname; struct addrinfo *ai_next; };
struct hostent { char *h_name; char **h_aliases; int h_addrtype; int h_length; char **h_addr_list; };
#define AI_PASSIVE 1
#define h_addr h_addr_list[0]
#define AI_CANONNAME 2
#define AI_NUMERICHOST 4
#define NI_NUMERICHOST 1
#define EAI_NONAME -2
#define EAI_FAIL -4
#define EAI_FAMILY -6
#define EAI_SERVICE -8
#define EAI_SOCKTYPE -7
int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res);
void freeaddrinfo(struct addrinfo *res);
const char *gai_strerror(int errcode);
struct hostent *gethostbyname(const char *name);
struct hostent *gethostbyaddr(const void *addr, unsigned int len, int type);
struct servent { char *s_name; char **s_aliases; int s_port; char *s_proto; };
struct servent *getservbyname(const char *name, const char *proto);
#endif
