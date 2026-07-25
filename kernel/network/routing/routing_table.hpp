#pragma once

#include "network_types.hpp"

namespace blockos::network {

class RoutingTable {
public:
    void reset();
    bool add(const NetworkRoute& route);
    bool clear();
    const NetworkRoute* route(std::size_t index) const;
    std::size_t count() const { return count_; }

private:
    NetworkRoute routes_[NET_MAX_ROUTES]{};
    std::size_t count_ = 0;
};

} // namespace blockos::network
