kernel/network/network_types.hpp
#pragma once

#include <cstdint>
#include <cstddef>

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
    DHCP = 0,
    Static = 1,
    LinkLocal = 2,
    Disabled = 3
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
static constexpr std::size_t NET_MAX_ROUTES = 16;
static constexpr std::size_t NET_MAX_ADAPTERS = 16;
static constexpr std::size_t NET_MAX_DNS = 4;

}
kernel/network/network_adapter.hpp
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

}
kernel/network/network_manager.hpp
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

    static bool same_adapter(const INetworkAdapter& a, const INetworkAdapter& b);
    std::size_t find_adapter_index(const INetworkAdapter& adapter) const;
};

}
kernel/network/network_manager.cpp
#include "network_manager.hpp"
#include <cstring>

namespace blockos::network {

NetworkManager::NetworkManager(Hooks hooks)
    : hooks_(hooks) {}

void NetworkManager::log(const char* msg) const {
    if (hooks_.log) hooks_.log(hooks_.user, msg);
}

bool NetworkManager::same_adapter(const INetworkAdapter& a, const INetworkAdapter& b) {
    return a.mac_address() == b.mac_address() && std::strcmp(a.name(), b.name()) == 0;
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

INetworkAdapter* NetworkManager::primary() {
    return primary_;
}

const INetworkAdapter* NetworkManager::primary() const {
    return primary_;
}

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

    if (mode_ == NetworkMode::Disabled) {
        log("[NET] disabled");
        return false;
    }

    if (!primary_) {
        log("[NET] no primary adapter");
        return false;
    }

    if (!primary_->bring_up()) {
        log("[NET] primary bring_up failed");
        ++stats_.tx_errors;
        return false;
    }

    up_ = true;
    log("[NET] up");
    return true;
}

bool NetworkManager::bring_down() {
    if (!up_) return true;

    if (primary_) {
        primary_->bring_down();
    }

    up_ = false;
    log("[NET] down");
    return true;
}

void NetworkManager::tick() {
    for (std::size_t i = 0; i < adapter_count_; ++i) {
        if (adapters_[i]) adapters_[i]->tick();
    }

    if (mode_ == NetworkMode::DHCP && up_ && primary_) {
        /* DHCP state machine hook helye */
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

}
kernel/network/dhcp_client.hpp
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

}
kernel/network/dhcp_client.cpp
#include "dhcp_client.hpp"
#include <cstring>

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
        case State::Idle:
            break;

        case State::Discover:
            /* itt jönne a DHCP DISCOVER küldés */
            state_ = State::Offer;
            break;

        case State::Offer:
            /* itt jönne az OFFER feldolgozása */
            state_ = State::Request;
            break;

        case State::Request:
            /* itt jönne a REQUEST */
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

}
kernel/network/dns_resolver.hpp
#pragma once

#include "network_types.hpp"

namespace blockos::network {

class DnsResolver {
public:
    static constexpr std::size_t MAX_ENTRIES = 32;
    static constexpr std::size_t MAX_NAME = 128;

    struct Entry {
        char host[MAX_NAME]{};
        IPv4Address addr{};
        bool used = false;
    };

    void reset();
    bool add(const char* host, const IPv4Address& addr);
    bool resolve_cached(const char* host, IPv4Address& out) const;
    bool remove(const char* host);

private:
    Entry entries_[MAX_ENTRIES]{};

    static void copy_text(char* dst, std::size_t dst_sz, const char* src);
    static bool streq(const char* a, const char* b);
};

}
kernel/network/dns_resolver.cpp
#include "dns_resolver.hpp"
#include <cstring>

namespace blockos::network {

void DnsResolver::copy_text(char* dst, std::size_t dst_sz, const char* src) {
    if (!dst || dst_sz == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    std::size_t n = std::strlen(src);
    if (n >= dst_sz) n = dst_sz - 1;
    if (n > 0) std::memcpy(dst, src, n);
    dst[n] = '\0';
}

bool DnsResolver::streq(const char* a, const char* b) {
    if (!a || !b) return false;
    return std::strcmp(a, b) == 0;
}

void DnsResolver::reset() {
    std::memset(entries_, 0, sizeof(entries_));
}

bool DnsResolver::add(const char* host, const IPv4Address& addr) {
    if (!host || !host[0]) return false;

    for (auto& e : entries_) {
        if (e.used && streq(e.host, host)) {
            e.addr = addr;
            return true;
        }
    }

    for (auto& e : entries_) {
        if (!e.used) {
            e.used = true;
            e.addr = addr;
            copy_text(e.host, sizeof(e.host), host);
            return true;
        }
    }

    return false;
}

bool DnsResolver::resolve_cached(const char* host, IPv4Address& out) const {
    if (!host || !host[0]) return false;

    for (const auto& e : entries_) {
        if (e.used && streq(e.host, host)) {
            out = e.addr;
            return true;
        }
    }

    return false;
}

bool DnsResolver::remove(const char* host) {
    if (!host || !host[0]) return false;

    for (auto& e : entries_) {
        if (e.used && streq(e.host, host)) {
            e = {};
            return true;
        }
    }

    return false;
}

}
kernel/network/network_service.hpp
#pragma once

#include "network_manager.hpp"
#include "dhcp_client.hpp"
#include "dns_resolver.hpp"

namespace blockos::network {

class NetworkService {
public:
    struct Hooks {
        void* user = nullptr;
        void (*log)(void* user, const char* msg) = nullptr;
    };

    explicit NetworkService(NetworkManager& nm, Hooks hooks = {});

    NetworkManager& manager();
    DnsResolver& dns();
    DhcpClient& dhcp();

    bool start();
    void stop();
    void tick();

    bool bring_up_dhcp();
    bool set_static(const IPv4Config& cfg);

    bool add_dns(const IPv4Address& server);
    bool resolve(const char* host, IPv4Address& out) const;

private:
    NetworkManager& nm_;
    Hooks hooks_{};
    DnsResolver dns_;
    DhcpClient dhcp_;
    bool running_ = false;

    void log(const char* msg) const;
};

}
kernel/network/network_service.cpp
#include "network_service.hpp"

namespace blockos::network {

NetworkService::NetworkService(NetworkManager& nm, Hooks hooks)
    : nm_(nm), hooks_(hooks) {}

void NetworkService::log(const char* msg) const {
    if (hooks_.log) hooks_.log(hooks_.user, msg);
}

NetworkManager& NetworkService::manager() {
    return nm_;
}

DnsResolver& NetworkService::dns() {
    return dns_;
}

DhcpClient& NetworkService::dhcp() {
    return dhcp_;
}

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

}
Példa bekötés a kernelbe
#include "kernel/network/network_manager.hpp"
#include "kernel/network/network_service.hpp"

using namespace blockos::network;

class VirtioNetAdapter : public INetworkAdapter {
public:
    const char* name() const override { return "virtio-net0"; }
    MacAddress mac_address() const override { return mac_; }
    LinkState link_state() const override { return LinkState::Up; }
    std::uint32_t mtu() const override { return 1500; }

    bool bring_up() override { up_ = true; return true; }
    bool bring_down() override { up_ = false; return true; }

    bool send_frame(const void* data, std::size_t len) override {
        (void)data;
        (void)len;
        return up_;
    }

    std::size_t poll_frame(void* out, std::size_t max) override {
        (void)out;
        (void)max;
        return 0;
    }

    void tick() override {}

private:
    MacAddress mac_{{0x02, 0x12, 0x34, 0x56, 0x78, 0x9A}};
    bool up_ = false;
};

static void kernel_log(void*, const char* msg) {
    (void)msg;
    // Print("%s\n", msg);
}

void init_network()
{
    static VirtioNetAdapter eth0;
    NetworkManager nm({nullptr, &kernel_log});
    nm.add_adapter(eth0);
    nm.set_primary(eth0);

    NetworkService svc(nm, {nullptr, &kernel_log});
    svc.start();

    IPv4Config cfg;
    cfg.address = IPv4Address{{192, 168, 1, 50}};
    cfg.netmask = IPv4Address{{255, 255, 255, 0}};
    cfg.gateway = IPv4Address{{192, 168, 1, 1}};
    cfg.dns = IPv4Address{{1, 1, 1, 1}};

    svc.set_static(cfg);
    svc.add_dns(cfg.dns);
}
