#pragma once

#include "network_types.hpp"

namespace blockos::network {

class DhcpClient {
public:
    enum class State : std::uint8_t {
        Idle = 0,
        Discover,
        Offer,
        Request,
        Bound,
        Failed
    };

    struct Result {
        IPv4Config config{};
        bool success = false;
    };

    DhcpClient();

    void reset();
    void start();
    State state() const { return state_; }
    void tick();

    bool has_result() const { return result_ready_; }
    Result result() const { return result_; }
    void consume_result();

private:
    State state_ = State::Idle;
    Result result_{};
    bool result_ready_ = false;
    std::uint32_t lease_time_ = 0;

    void set_default_result();
};

} // namespace blockos::network
