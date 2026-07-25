#pragma once

#include "network_adapter.hpp"

namespace blockos::network {

class VirtioNetAdapter : public INetworkAdapter {
public:
    const char* name() const override { return "virtio-net"; }
    MacAddress mac_address() const override { return mac_; }
    LinkState link_state() const override { return up_ ? LinkState::Up : LinkState::Down; }
    std::uint32_t mtu() const override { return 1500; }

    bool bring_up() override;
    bool bring_down() override;

    bool send_frame(const void* data, std::size_t len) override;
    std::size_t poll_frame(void* out, std::size_t max) override;

    void tick() override;

private:
    MacAddress mac_{{0x02, 0x12, 0x34, 0x56, 0x78, 0x9A}};
    bool up_ = false;
};

} // namespace blockos::network
