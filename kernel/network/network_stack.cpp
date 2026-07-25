#include "network_stack.hpp"

namespace blockos::network {

NetworkStack::NetworkStack(NetworkManager& nm)
    : nm_(nm), service_(nm), sockets_(nm) {}

NetworkManager& NetworkStack::manager() { return nm_; }
NetworkService& NetworkStack::service() { return service_; }
SocketManager& NetworkStack::sockets() { return sockets_; }
RoutingTable& NetworkStack::routes() { return routes_; }
Firewall& NetworkStack::firewall() { return firewall_; }
ArpCache& NetworkStack::arp() { return arp_; }

} // namespace blockos::network
