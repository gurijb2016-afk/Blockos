#include "routing_table.hpp"
#include <cstring>

namespace blockos::network {

void RoutingTable::reset() {
    count_ = 0;
    std::memset(routes_, 0, sizeof(routes_));
}

bool RoutingTable::add(const NetworkRoute& route) {
    if (count_ >= NET_MAX_ROUTES) return false;
    routes_[count_++] = route;
    return true;
}

bool RoutingTable::clear() {
    reset();
    return true;
}

const NetworkRoute* RoutingTable::route(std::size_t index) const {
    if (index >= count_) return nullptr;
    return &routes_[index];
}

} // namespace blockos::network
