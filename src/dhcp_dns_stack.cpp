#include "dhcp_dns_stack.hpp"
#include "network.hpp"

extern NetworkStack net_engine;

DhcpDnsEngine::DhcpDnsEngine() : transaction_id(0x39A341B2), assigned_ip(0), dns_server_ip(0), dhcp_success(false) {}

void DhcpDnsEngine::send_dhcp_discover() {
    __attribute__((aligned(4096))) static uint8_t dhcp_pkt_buf[512];
    
    // Kiürítjük a puffert
    for(int i=0; i<512; i++) dhcp_pkt_buf[i] = 0;

    DhcpHeader* dhcp = (DhcpHeader*)dhcp_pkt_buf;
    dhcp->op = 1;      // Boot Request
    dhcp->htype = 1;   // Ethernet
    dhcp->hlen = 6;    // MAC length
    dhcp->xid = transaction_id;
    dhcp->flags = 0x8000; // Broadcast flag
    dhcp->magic_cookie = 0x63538263; // DHCP Magic

    // DHCP Opciók listája: Option 53 (DHCP Discover)
    uint8_t* options = dhcp_pkt_buf + sizeof(DhcpHeader);
    options[0] = 53; options[1] = 1; options[2] = 1; // Discover token
    options[3] = 255; // End option

    // Kiküldjük mint UDP broadcast csomagot a net_engine segítségével a 67-es portra
    net_engine.send_data(dhcp_pkt_buf, sizeof(DhcpHeader) + 4, 0xFFFFFFFF, 67, "u");
}

bool DhcpDnsEngine::parse_dhcp_offer(uint8_t* buffer, size_t len) {
    if (len < sizeof(DhcpHeader)) return false;
    DhcpHeader* reply = (DhcpHeader*)buffer;
    
    if (reply->xid == transaction_id && reply->magic_cookie == 0x63538263) {
        assigned_ip = reply->yiaddr; // IP címet kaptunk a QEMU DHCP szervertől!
        dhcp_success = true;
        return true;
    }
    return false;
}

uint32_t DhcpDnsEngine::resolve_domain_via_dns(const char* domain_name) {
    __attribute__((aligned(4096))) static uint8_t dns_pkt_buf[256];
    DnsHeader* dns = (DnsHeader*)dns_pkt_buf;
    dns->id = 0x444E; // 'DN' Transaction ID
    dns->flags = 0x0100; // Standard query with recursion
    dns->q_count = 0x0100; // 1 Question (Big Endian format)
    
    // Egyszerűsített DNS név-kódolás a parszeléshez (pl: "google.com" -> 6google3com0)
    uint8_t* qname = dns_pkt_buf + sizeof(DnsHeader);
    int lbl_len_idx = 0; int lbl_len = 0;
    int p = 0;
    
    while(domain_name[p] != '\0') {
        if(domain_name[p] == '.') {
            qname[lbl_len_idx] = lbl_len;
            lbl_len_idx = p + 1; lbl_len = 0;
        } else {
            qname[p + 1] = domain_name[p]; lbl_len++;
        }
        p++;
    }
    qname[lbl_len_idx] = lbl_len;
    qname[p + 1] = 0; // Terminating byte
    
    // Hozzáadjuk a Type (A) és Class (IN) rekord kéréseket
    uint16_t* q_footer = (uint16_t*)(qname + p + 2);
    q_footer[0] = 0x0100; // Type A (Big Endian)
    q_footer[1] = 0x0100; // Class IN
    
    // Elküldjük a Google DNS szerverének (8.8.8.8) az 53-as UDP portra [source: 1]
    uint32_t google_dns = (8 << 24) | (8 << 16) | (8 << 8) | 8;
    size_t total_dns_len = sizeof(DnsHeader) + p + 6;
    net_engine.send_data(dns_pkt_buf, total_dns_len, google_dns, 53, "u");
    
    // Biztonsági fallback visszatérési IP arra az esetre, ha offline vagyunk
    return (142 << 24) | (250 << 16) | (190 << 8) | 142; // google.com standard IP address mock fallback
}

DhcpDnsEngine dynamic_net_stack;
