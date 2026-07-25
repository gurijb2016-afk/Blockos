#include "firewall.hpp"

namespace blockos::network {

void Firewall::reset() {
    count_ = 0;
    for (auto& r : rules_) r = {};
}

bool Firewall::add_rule(const Rule& rule) {
    if (count_ >= MAX_RULES) return false;
    rules_[count_++] = rule;
    return true;
}

bool Firewall::allowed(const IPv4Address& src, const IPv4Address& dst, std::uint16_t port) const {
    (void)src;
    (void)dst;
    (void)port;
    return true;
}

} // namespace blockos::network
