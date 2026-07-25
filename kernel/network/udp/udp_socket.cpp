#include "udp_socket.hpp"

namespace blockos::network {

void UdpSocket::reset() {
    port_ = 0;
}

bool UdpSocket::bind(std::uint16_t port) {
    port_ = port;
    return true;
}

bool UdpSocket::send(const void* data, std::size_t len) {
    (void)data;
    (void)len;
    return port_ != 0;
}

} // namespace blockos::network
