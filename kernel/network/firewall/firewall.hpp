#pragma once

#include "network_types.hpp"

namespace blockos::network {

class Firewall {
public:
    enum class Action : std::uint8_t {
        Allow = 0,
        Deny = 1
    };

    struct Rule {
        bool used = false;
        IPv4Address src{};
        IPv4Address dst{};
        std::uint16_t port = 0;
        Action action = Action::Allow;
    };

    void reset();
    bool add_rule(const Rule& rule);
    bool allowed(const IPv4Address& src, const IPv4Address& dst, std::uint16_t port) const;

private:
    static constexpr std::size_t MAX_RULES = 64;
    Rule rules_[MAX_RULES]{};
    std::size_t count_ = 0;
};

} // namespace blockos::network
