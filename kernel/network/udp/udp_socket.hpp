#pragma once

#include "network_types.hpp"

namespace blockos::network {

class UdpSocket {
public:
    void reset();
    bool bind(std::uint16_t port);
    bool send(const void* data, std::size_t len);

private:
    std::uint16_t port_ = 0;
};

} // namespace blockos::network
