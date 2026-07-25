#pragma once

#include <cstddef>
#include <cstdint>

namespace blockos::network {

struct MacAddress {
    std::uint8_t bytes[6]{};

    bool operator==(const MacAddress& other) const {
        for (int i = 0; i < 6; ++i) {
            if (bytes[i] != other.bytes[i]) return false;
        }
        return true;
    }

    bool is_zero() const {
        for (int i = 0; i < 6; ++i) {
            if (bytes[i] != 0) return false;
        }
        return true;
    }
};

struct IPv4Address {
    std::uint8_t bytes[4]{};

    bool operator==(const IPv4Address& other) const {
        for (int i = 0; i < 4; ++i) {
            if (bytes[i] != other.bytes[i]) return false;
        }
        return true;
    }

    bool is_zero() const {
        for (int i = 0; i < 4; ++i) {
            if (bytes[i] != 0) return false;
        }
        return true;
    }

    std::uint32_t to_u32_be() const {
        return (std::uint32_t(bytes[0]) << 24) |
               (std::uint32_t(bytes[1]) << 16) |
               (std::uint32_t(bytes[2]) << 8)  |
               (std::uint32_t(bytes[3]));
    }

    static IPv4Address from_u32_be(std::uint32_t v) {
        IPv4Address ip;
        ip.bytes[0] = (v >> 24) & 0xFF;
        ip.bytes[1] = (v >> 16) & 0xFF;
        ip.bytes[2] = (v >> 8) & 0xFF;
        ip.bytes[3] = v & 0xFF;
        return ip;
    }
};

enum class LinkState : std::uint8_t {
    Down = 0,
    Up = 1,
    Unknown = 2
};

enum class NetworkMode : std::uint8_t {
    Disabled = 0,
    DHCP = 1,
    Static = 2,
    LinkLocal = 3
};

enum class SocketState : std::uint8_t {
    Closed = 0,
    Bound,
    Listening,
    SynSent,
    Established,
    Shutdown
};

struct IPv4Config {
    IPv4Address address{};
    IPv4Address netmask{};
    IPv4Address gateway{};
    IPv4Address dns{};
};

struct NetworkStats {
    std::uint64_t tx_frames = 0;
    std::uint64_t rx_frames = 0;
    std::uint64_t tx_bytes = 0;
    std::uint64_t rx_bytes = 0;
    std::uint64_t tx_errors = 0;
    std::uint64_t rx_errors = 0;
};

struct NetworkRoute {
    IPv4Address destination{};
    IPv4Address gateway{};
    IPv4Address netmask{};
    bool default_route = false;
};

static constexpr std::size_t NET_NAME_MAX = 64;
static constexpr std::size_t NET_MAX_ROUTES = 32;
static constexpr std::size_t NET_MAX_ADAPTERS = 16;
static constexpr std::size_t NET_MAX_DNS = 4;
static constexpr std::size_t NET_MAX_SOCKETS = 256;
static constexpr std::size_t NET_MAX_QUEUE = 64;
static constexpr std::size_t NET_MAX_PAYLOAD = 1500;

} // namespace blockos::network
