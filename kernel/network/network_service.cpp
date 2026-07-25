#include "network_service.hpp"

namespace blockos::network {

NetworkService::NetworkService(NetworkManager& nm, Hooks hooks)
    : nm_(nm), hooks_(hooks) {}

void NetworkService::log(const char* msg) const {
    if (hooks_.log) hooks_.log(hooks_.user, msg);
}

NetworkManager& NetworkService::manager() { return nm_; }
DnsResolver& NetworkService::dns() { return dns_; }
DhcpClient& NetworkService::dhcp() { return dhcp_; }

bool NetworkService::start() {
    running_ = true;
    log("[NETSVC] start");
    return true;
}

void NetworkService::stop() {
    running_ = false;
    nm_.bring_down();
    log("[NETSVC] stop");
}

void NetworkService::tick() {
    if (!running_) return;
    nm_.tick();
    dhcp_.tick();

    if (dhcp_.has_result()) {
        const auto r = dhcp_.result();
        if (r.success) {
            nm_.configure_static(r.config);
            dns_.add("default", r.config.dns);
            nm_.set_mode(NetworkMode::Static);
            nm_.bring_up();
            log("[NETSVC] DHCP bound");
        }
        dhcp_.consume_result();
    }
}

bool NetworkService::bring_up_dhcp() {
    nm_.set_mode(NetworkMode::DHCP);
    dhcp_.start();
    return nm_.bring_up();
}

bool NetworkService::set_static(const IPv4Config& cfg) {
    nm_.configure_static(cfg);
    nm_.set_mode(NetworkMode::Static);
    return nm_.bring_up();
}

bool NetworkService::add_dns(const IPv4Address& server) {
    return dns_.add("dns", server);
}

bool NetworkService::resolve(const char* host, IPv4Address& out) const {
    return dns_.resolve_cached(host, out);
}

} // namespace blockos::network
