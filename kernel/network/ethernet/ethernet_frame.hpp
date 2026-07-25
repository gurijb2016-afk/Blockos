#pragma once

#include "network_types.hpp"

namespace blockos::network {

struct EthernetFrame {
    MacAddress dst{};
    MacAddress src{};
    std::uint16_t ethertype = 0;
    const void* payload = nullptr;
    std::size_t payload_len = 0;
};

} // namespace blockos::network
