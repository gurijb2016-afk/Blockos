#pragma once
#include <stdint.h>
#include <stddef.h>

struct DhcpHeader {
    uint8_t  op; uint8_t htype; uint8_t hlen; uint8_t hops;
    uint32_t xid; uint16_t secs; uint16_t flags;
    uint32_t ciaddr; uint32_t yiaddr; uint32_t siaddr; uint32_t giaddr;
    uint8_t  chaddr[16]; char sname[64]; char file[128];
    uint32_t magic_cookie;
} __attribute__((packed));

struct DnsHeader {
    uint16_t id; uint16_t flags;
    uint16_t q_count; uint16_t ans_count;
    uint16_t auth_count; uint16_t add_count;
} __attribute__((packed));

class DhcpDnsEngine {
private:
    uint32_t transaction_id;
    uint32_t assigned_ip;
    uint32_t dns_server_ip;
    bool     dhcp_success;

public:
    DhcpDnsEngine();
    void send_dhcp_discover();
    bool parse_dhcp_offer(uint8_t* buffer, size_t len);
    uint32_t resolve_domain_via_dns(const char* domain_name);
    uint32_t get_assigned_ip() { return assigned_ip; }
};

extern DhcpDnsEngine dynamic_net_stack;
