#pragma once

#include "network_manager.hpp"
#include "network_service.hpp"
#include "socket/socket_manager.hpp"
#include "routing/routing_table.hpp"
#include "firewall/firewall.hpp"
#include "arp/arp_cache.hpp"

namespace blockos::network {

class NetworkStack {
public:
    explicit NetworkStack(NetworkManager& nm);

    NetworkManager& manager();
    NetworkService& service();
    SocketManager& sockets();
    RoutingTable& routes();
    Firewall& firewall();
    ArpCache& arp();

private:
    NetworkManager& nm_;
    NetworkService service_;
    SocketManager sockets_;
    RoutingTable routes_;
    Firewall firewall_;
    ArpCache arp_;
};

} // namespace blockos::network
