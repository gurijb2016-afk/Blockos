#include "dhcp_client.hpp"

namespace blockos::network {

DhcpClient::DhcpClient() {
    reset();
}

void DhcpClient::set_default_result() {
    result_.config.address = IPv4Address{{169, 254, 1, 10}};
    result_.config.netmask = IPv4Address{{255, 255, 0, 0}};
    result_.config.gateway = IPv4Address{{0, 0, 0, 0}};
    result_.config.dns = IPv4Address{{1, 1, 1, 1}};
    result_.success = false;
}

void DhcpClient::reset() {
    state_ = State::Idle;
    result_ = {};
    result_ready_ = false;
    lease_time_ = 0;
}

void DhcpClient::start() {
    state_ = State::Discover;
    result_ready_ = false;
    lease_time_ = 0;
}

void DhcpClient::tick() {
    switch (state_) {
        case State::Idle: break;
        case State::Discover: state_ = State::Offer; break;
        case State::Offer: state_ = State::Request; break;
        case State::Request:
            set_default_result();
            result_.success = true;
            state_ = State::Bound;
            result_ready_ = true;
            lease_time_ = 3600;
            break;
        case State::Bound:
            if (lease_time_ > 0) --lease_time_;
            break;
        case State::Failed:
            break;
    }
}

void DhcpClient::consume_result() {
    result_ready_ = false;
}

} // namespace blockos::network
