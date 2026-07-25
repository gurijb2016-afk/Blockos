#pragma once

#include "network_types.hpp"

namespace blockos::network {

struct IPv4Packet {
    IPv4Address src{};
    IPv4Address dst{};
    std::uint8_t protocol = 0;
    const void* payload = nullptr;
    std::size_t payload_len = 0;
};

} // namespace blockos::network
