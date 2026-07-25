#pragma once

#include "network_types.hpp"

namespace blockos::network {

class ArpCache {
public:
    static constexpr std::size_t MAX_ENTRIES = 64;

    struct Entry {
        bool used = false;
        IPv4Address ip{};
        MacAddress mac{};
    };

    void reset();
    bool add(const IPv4Address& ip, const MacAddress& mac);
    bool lookup(const IPv4Address& ip, MacAddress& out) const;

private:
    Entry entries_[MAX_ENTRIES]{};
};

} // namespace blockos::network
