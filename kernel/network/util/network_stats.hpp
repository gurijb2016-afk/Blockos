#pragma once

#include "network_types.hpp"

namespace blockos::network {

class NetworkStatsView {
public:
    void reset();
    void add_tx(std::size_t bytes);
    void add_rx(std::size_t bytes);

    NetworkStats stats() const { return stats_; }

private:
    NetworkStats stats_{};
};

} // namespace blockos::network
