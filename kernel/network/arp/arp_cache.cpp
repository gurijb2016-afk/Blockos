#include "arp_cache.hpp"
#include <cstring>

namespace blockos::network {

void ArpCache::reset() {
    std::memset(entries_, 0, sizeof(entries_));
}

bool ArpCache::add(const IPv4Address& ip, const MacAddress& mac) {
    for (auto& e : entries_) {
        if (e.used && e.ip == ip) {
            e.mac = mac;
            return true;
        }
    }
    for (auto& e : entries_) {
        if (!e.used) {
            e.used = true;
            e.ip = ip;
            e.mac = mac;
            return true;
        }
    }
    return false;
}

bool ArpCache::lookup(const IPv4Address& ip, MacAddress& out) const {
    for (const auto& e : entries_) {
        if (e.used && e.ip == ip) {
            out = e.mac;
            return true;
        }
    }
    return false;
}

} // namespace blockos::network
