#pragma once

#include "network_types.hpp"

namespace blockos::network {

struct IcmpPacket {
    std::uint8_t type = 0;
    std::uint8_t code = 0;
    const void* payload = nullptr;
    std::size_t payload_len = 0;
};

} // namespace blockos::network
