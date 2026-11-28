#pragma once
#include <stdint.h>
#include <stddef.h>
#include "net.hpp"

namespace blockos::net {

enum : int {
    SOCK_STREAM = 1,
    SOCK_DGRAM = 2
};

int socket(int domain, int type, int protocol);
int bind(int fd, uint16_t port);
int connect(int fd, const IPv4Address& addr, uint16_t port);
int listen(int fd, int backlog);
int accept(int fd);
int send(int fd, const void* buf, size_t len);
int recv(int fd, void* buf, size_t len);
int close(int fd);

} // namespace blockos::net
