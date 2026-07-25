#pragma once

#include "network_types.hpp"

namespace blockos::network {

class TcpSocket {
public:
    void reset();
    bool bind(std::uint16_t port);
    bool connect(std::uint16_t port);
    bool listen(int backlog);
    SocketState state() const { return state_; }

private:
    SocketState state_ = SocketState::Closed;
    std::uint16_t port_ = 0;
};

} // namespace blockos::network
