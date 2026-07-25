#pragma once

#include <cstdint>
#include <cstddef>

namespace blockos::network {

struct IPv6Address {
    std::uint8_t bytes[16]{};
};

struct IPv6Packet {
    IPv6Address src{};
    IPv6Address dst{};
    std::uint8_t next_header = 0;
    const void* payload = nullptr;
    std::size_t payload_len = 0;
};

} // namespace blockos::network
