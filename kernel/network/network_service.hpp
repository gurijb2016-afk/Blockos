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

} // namespace blockos::network
