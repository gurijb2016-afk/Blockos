#include "tcp_socket.hpp"

namespace blockos::network {

void TcpSocket::reset() {
    state_ = SocketState::Closed;
    port_ = 0;
}

bool TcpSocket::bind(std::uint16_t port) {
    port_ = port;
    state_ = SocketState::Bound;
    return true;
}

bool TcpSocket::connect(std::uint16_t port) {
    port_ = port;
    state_ = SocketState::Established;
    return true;
}

bool TcpSocket::listen(int backlog) {
    (void)backlog;
    state_ = SocketState::Listening;
    return true;
}

} // namespace blockos::network
