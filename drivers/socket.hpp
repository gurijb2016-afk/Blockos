#pragma once
#include <stdint.h>
#include <stddef.h>

/* Minimal socket placeholder so drivers/network_socket.cpp can compile.
   Replace with full implementation later (BSD-like API or project-specific). */

enum SocketState {
    SOCKET_STATE_CLOSED = 0,
    SOCKET_STATE_OPEN   = 1,
    SOCKET_STATE_LISTEN = 2,
};

struct Socket {
    int fd;
    int family;
    int type;
    int protocol;
    SocketState state;

    // Simple buffers so code that touches them won't fail at link time.
    uint8_t rx_buf[1500];
    size_t  rx_len;
    uint8_t tx_buf[1500];
    size_t  tx_len;
};

class SocketManager {
public:
    // Create a new socket, returns fd >=0 or -1 on error
    static int create(int family, int type, int protocol);

    // Close socket
    static int close(int fd);

    // Send/recv (minimal signatures)
    static int send(int fd, const void* data, size_t len);
    static int recv(int fd, void* buf, size_t len);
};
