#pragma once

#include "network_adapter.hpp"

namespace blockos::network {

class WifiAdapter : public INetworkAdapter {
public:
    const char* name() const override { return "wifi"; }
    MacAddress mac_address() const override { return mac_; }
    LinkState link_state() const override { return up_ ? LinkState::Up : LinkState::Down; }
    std::uint32_t mtu() const override { return 1500; }

    bool bring_up() override;
    bool bring_down() override;
    bool send_frame(const void* data, std::size_t len) override;
    std::size_t poll_frame(void* out, std::size_t max) override;
    void tick() override;

private:
    MacAddress mac_{{0x02, 0x55, 0x49, 0x46, 0x49, 0x01}};
    bool up_ = false;
};

} // namespace blockos::network
