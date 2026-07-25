#include "network_stats.hpp"

namespace blockos::network {

void NetworkStatsView::reset() {
    stats_ = {};
}

void NetworkStatsView::add_tx(std::size_t bytes) {
    ++stats_.tx_frames;
    stats_.tx_bytes += bytes;
}

void NetworkStatsView::add_rx(std::size_t bytes) {
    ++stats_.rx_frames;
    stats_.rx_bytes += bytes;
}

} // namespace blockos::network
