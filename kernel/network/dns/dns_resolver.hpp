#pragma once

#include "network_types.hpp"

namespace blockos::network {

class DnsResolver {
public:
    static constexpr std::size_t MAX_ENTRIES = 32;
    static constexpr std::size_t MAX_NAME = 128;

    struct Entry {
        char host[MAX_NAME]{};
        IPv4Address addr{};
        bool used = false;
    };

    void reset();
    bool add(const char* host, const IPv4Address& addr);
    bool resolve_cached(const char* host, IPv4Address& out) const;
    bool remove(const char* host);

private:
    Entry entries_[MAX_ENTRIES]{};

    static void copy_text(char* dst, std::size_t dst_sz, const char* src);
    static bool streq(const char* a, const char* b);
};

} // namespace blockos::network
