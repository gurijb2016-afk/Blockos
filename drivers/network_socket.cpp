#include "socket.hpp"

#define MAX_SOCKETS 16

static Socket sockets[MAX_SOCKETS];
static int socket_count = 0;


int SocketManager::create(
    int family,
    int type,
    int protocol
)
{
    if(socket_count >= MAX_SOCKETS)
        return -1;


    Socket& sock = sockets[socket_count];

    sock.fd = socket_count;
    sock.family = family;
    sock.type = type;
    sock.protocol = protocol;
    sock.state = SOCKET_STATE_CLOSED;

    socket_count++;

    return sock.fd;
}
