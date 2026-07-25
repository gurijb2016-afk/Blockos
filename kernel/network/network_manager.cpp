#include "network_manager.hpp"
#include <cstring>

namespace blockos::network {

NetworkManager::NetworkManager(Hooks hooks)
    : hooks_(hooks) {}

void NetworkManager::log(const char* msg) const {
    if (hooks_.log) hooks_.log(hooks_.user, msg);
}

std::size_t NetworkManager::find_adapter_index(const INetworkAdapter& adapter) const {
    for (std::size_t i = 0; i < adapter_count_; ++i) {
        if (adapters_[i] == &adapter) return i;
    }
    return static_cast<std::size_t>(-1);
}

bool NetworkManager::add_adapter(INetworkAdapter& adapter) {
    if (adapter_count_ >= NET_MAX_ADAPTERS) return false;
    if (find_adapter_index(adapter) != static_cast<std::size_t>(-1)) return true;

    adapters_[adapter_count_++] = &adapter;
    if (!primary_) primary_ = &adapter;

    log("[NET] adapter added");
    return true;
}

bool NetworkManager::remove_adapter(INetworkAdapter& adapter) {
    std::size_t idx = find_adapter_index(adapter);
    if (idx == static_cast<std::size_t>(-1)) return false;

    for (std::size_t i = idx; i + 1 < adapter_count_; ++i) {
        adapters_[i] = adapters_[i + 1];
    }
    adapters_[adapter_count_ - 1] = nullptr;
    --adapter_count_;

    if (primary_ == &adapter) {
        primary_ = adapter_count_ ? adapters_[0] : nullptr;
    }
    return true;
}

INetworkAdapter* NetworkManager::adapter(std::size_t index) {
    if (index >= adapter_count_) return nullptr;
    return adapters_[index];
}

const INetworkAdapter* NetworkManager::adapter(std::size_t index) const {
    if (index >= adapter_count_) return nullptr;
    return adapters_[index];
}

bool NetworkManager::set_primary(INetworkAdapter& adapter) {
    if (find_adapter_index(adapter) == static_cast<std::size_t>(-1)) return false;
    primary_ = &adapter;
    return true;
}

INetworkAdapter* NetworkManager::primary() { return primary_; }
const INetworkAdapter* NetworkManager::primary() const { return primary_; }

bool NetworkManager::set_mode(NetworkMode mode) {
    mode_ = mode;
    return true;
}

bool NetworkManager::configure_static(const IPv4Config& cfg) {
    ipv4_ = cfg;
    mode_ = NetworkMode::Static;
    return true;
}

bool NetworkManager::add_route(const NetworkRoute& route) {
    if (route_count_ >= NET_MAX_ROUTES) return false;
    routes_[route_count_++] = route;
    return true;
}

bool NetworkManager::clear_routes() {
    route_count_ = 0;
    std::memset(routes_, 0, sizeof(routes_));
    return true;
}

const NetworkRoute* NetworkManager::route(std::size_t index) const {
    if (index >= route_count_) return nullptr;
    return &routes_[index];
}

bool NetworkManager::add_dns(const IPv4Address& server) {
    if (dns_count_ >= NET_MAX_DNS) return false;
    dns_[dns_count_++] = server;
    return true;
}

bool NetworkManager::clear_dns() {
    dns_count_ = 0;
    std::memset(dns_, 0, sizeof(dns_));
    return true;
}

const IPv4Address* NetworkManager::dns_server(std::size_t index) const {
    if (index >= dns_count_) return nullptr;
    return &dns_[index];
}

bool NetworkManager::bring_up() {
    if (up_) return true;
    if (mode_ == NetworkMode::Disabled) return false;
    if (!primary_) return false;
    if (!primary_->bring_up()) {
        ++stats_.tx_errors;
        return false;
    }
    up_ = true;
    log("[NET] up");
    return true;
}

bool NetworkManager::bring_down() {
    if (!up_) return true;
    if (primary_) primary_->bring_down();
    up_ = false;
    log("[NET] down");
    return true;
}

void NetworkManager::tick() {
    for (std::size_t i = 0; i < adapter_count_; ++i) {
        if (adapters_[i]) adapters_[i]->tick();
    }
}

bool NetworkManager::send_raw(const void* data, std::size_t len) {
    if (!up_ || !primary_ || !data || len == 0) return false;
    if (!primary_->send_frame(data, len)) {
        ++stats_.tx_errors;
        return false;
    }
    ++stats_.tx_frames;
    stats_.tx_bytes += len;
    return true;
}

std::size_t NetworkManager::recv_raw(void* out, std::size_t max) {
    if (!up_ || !primary_ || !out || max == 0) return 0;
    std::size_t n = primary_->poll_frame(out, max);
    if (n == 0) return 0;
    ++stats_.rx_frames;
    stats_.rx_bytes += n;
    return n;
}

void NetworkManager::reset_stats() {
    stats_ = {};
}

} // namespace blockos::network
