#pragma once

#include "network_types.hpp"
#include "network_adapter.hpp"

namespace blockos::network {

class NetworkManager {
public:
    using LogFn = void (*)(void* user, const char* msg);

    struct Hooks {
        void* user = nullptr;
        LogFn log = nullptr;
    };

    explicit NetworkManager(Hooks hooks = {});

    bool add_adapter(INetworkAdapter& adapter);
    bool remove_adapter(INetworkAdapter& adapter);

    INetworkAdapter* adapter(std::size_t index);
    const INetworkAdapter* adapter(std::size_t index) const;
    std::size_t adapter_count() const { return adapter_count_; }

    bool set_primary(INetworkAdapter& adapter);
    INetworkAdapter* primary();
    const INetworkAdapter* primary() const;

    bool set_mode(NetworkMode mode);
    NetworkMode mode() const { return mode_; }

    bool configure_static(const IPv4Config& cfg);
    const IPv4Config& ipv4_config() const { return ipv4_; }

    bool add_route(const NetworkRoute& route);
    bool clear_routes();
    std::size_t route_count() const { return route_count_; }
    const NetworkRoute* route(std::size_t index) const;

    bool add_dns(const IPv4Address& server);
    bool clear_dns();
    std::size_t dns_count() const { return dns_count_; }
    const IPv4Address* dns_server(std::size_t index) const;

    bool bring_up();
    bool bring_down();
    bool is_up() const { return up_; }

    void tick();

    bool send_raw(const void* data, std::size_t len);
    std::size_t recv_raw(void* out, std::size_t max);

    NetworkStats stats() const { return stats_; }
    void reset_stats();

    void log(const char* msg) const;

private:
    Hooks hooks_{};

    INetworkAdapter* adapters_[NET_MAX_ADAPTERS]{};
    std::size_t adapter_count_ = 0;

    INetworkAdapter* primary_ = nullptr;

    NetworkMode mode_ = NetworkMode::Disabled;
    bool up_ = false;

    IPv4Config ipv4_{};
    NetworkRoute routes_[NET_MAX_ROUTES]{};
    std::size_t route_count_ = 0;

    IPv4Address dns_[NET_MAX_DNS]{};
    std::size_t dns_count_ = 0;

    NetworkStats stats_{};

    std::size_t find_adapter_index(const INetworkAdapter& adapter) const;
};

} // namespace blockos::network
