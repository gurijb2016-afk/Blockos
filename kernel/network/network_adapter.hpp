#pragma once

#include "network_types.hpp"

namespace blockos::network {

class INetworkAdapter {
public:
    virtual ~INetworkAdapter() = default;

    virtual const char* name() const = 0;
    virtual MacAddress mac_address() const = 0;
    virtual LinkState link_state() const = 0;
    virtual std::uint32_t mtu() const = 0;

    virtual bool bring_up() = 0;
    virtual bool bring_down() = 0;

    virtual bool send_frame(const void* data, std::size_t len) = 0;
    virtual std::size_t poll_frame(void* out, std::size_t max) = 0;

    virtual void tick() = 0;
};

} // namespace blockos::network
