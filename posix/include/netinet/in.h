#pragma once
#include <stdint.h>
typedef uint16_t in_port_t; typedef uint32_t in_addr_t;
struct in_addr { in_addr_t s_addr; };
struct sockaddr_in { uint16_t sin_family; in_port_t sin_port; struct in_addr sin_addr; unsigned char sin_zero[8]; };
#define INADDR_ANY ((in_addr_t)0)
#ifdef __cplusplus
extern "C" { uint16_t htons(uint16_t); uint16_t ntohs(uint16_t); uint32_t htonl(uint32_t); uint32_t ntohl(uint32_t); }
#endif
