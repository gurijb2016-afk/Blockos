#include "dns_resolver.hpp"
#include <cstring>

namespace blockos::network {

void DnsResolver::copy_text(char* dst, std::size_t dst_sz, const char* src) {
    if (!dst || dst_sz == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    std::size_t n = std::strlen(src);
    if (n >= dst_sz) n = dst_sz - 1;
    if (n > 0) std::memcpy(dst, src, n);
    dst[n] = '\0';
}

bool DnsResolver::streq(const char* a, const char* b) {
    if (!a || !b) return false;
    return std::strcmp(a, b) == 0;
}

void DnsResolver::reset() {
    std::memset(entries_, 0, sizeof(entries_));
}

bool DnsResolver::add(const char* host, const IPv4Address& addr) {
    if (!host || !host[0]) return false;

    for (auto& e : entries_) {
        if (e.used && streq(e.host, host)) {
            e.addr = addr;
            return true;
        }
    }

    for (auto& e : entries_) {
        if (!e.used) {
            e.used = true;
            e.addr = addr;
            copy_text(e.host, sizeof(e.host), host);
            return true;
        }
    }

    return false;
}

bool DnsResolver::resolve_cached(const char* host, IPv4Address& out) const {
    if (!host || !host[0]) return false;

    for (const auto& e : entries_) {
        if (e.used && streq(e.host, host)) {
            out = e.addr;
            return true;
        }
    }

    return false;
}

bool DnsResolver::remove(const char* host) {
    if (!host || !host[0]) return false;

    for (auto& e : entries_) {
        if (e.used && streq(e.host, host)) {
            e = {};
            return true;
        }
    }

    return false;
}

} // namespace blockos::network
