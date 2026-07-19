#pragma once
#include <stdint.h>
#include <stddef.h>

// --- VIRTIO-NET RING BUFFER STRUKTÚRÁK ---
struct VirtioNetDesc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed));

struct VirtioNetAvail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[128];
} __attribute__((packed));

struct VirtioNetUsedElem {
    uint32_t id;
    uint32_t len;
} __attribute__((packed));

struct VirtioNetUsed {
    uint16_t flags;
    uint16_t idx;
    VirtioNetUsedElem ring[128];
} __attribute__((packed));

struct VirtioNetHeader {
    uint8_t flags;
    uint8_t gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
    uint16_t num_buffers;
} __attribute__((packed));

// --- HÁLÓZATI PROTOKOLL STRUKTÚRÁK ---
struct EthernetHeader {
    uint8_t  dest_mac[6];
    uint8_t  src_mac[6];
    uint16_t ethertype; // 0x0800: IPv4, 0x0806: ARP
} __attribute__((packed));

struct ArpHeader {
    uint16_t hw_type;
    uint16_t proto_type;
    uint8_t  hw_len;
    uint8_t  proto_len;
    uint16_t opcode; // 1: Request, 2: Reply
    uint8_t  src_mac[6];
    uint32_t src_ip;
    uint8_t  dest_mac[6];
    uint32_t dest_ip;
} __attribute__((packed));

struct IpHeader {
    uint8_t  version_ihl; // Verzió (4) + Fejléc hossza (5)
    uint8_t  tos;
    uint16_t total_len;
    uint16_t id;
    uint16_t flags_fragment;
    uint8_t  ttl;
    uint8_t  protocol; // 1: ICMP, 6: TCP, 17: UDP
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dest_ip;
} __attribute__((packed));

struct UdpHeader {
    uint16_t src_port;
    uint16_t dest_port;
    uint16_t length;
    uint16_t checksum;
} __attribute__((packed));

struct TcpHeader {
    uint16_t src_port;
    uint16_t dest_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint16_t flags; // Data offset + Flags (SYN, ACK, FIN stb.)
    uint16_t window_size;
    uint16_t checksum;
    uint16_t urgent_ptr;
} __attribute__((packed));

// --- CORE NETWORK NETWORK CORE OS REQUISITES ---
class NetworkStack {
private:
    uint32_t local_ip;
    uint8_t  local_mac[6];
    uint32_t gateway_ip;
    uint8_t  server_mac[6];
    bool     server_mac_resolved;

    // VirtIO Gyűrű puffer allokációk (4KB-os hardveres határokra igazítva)
    __attribute__((aligned(4096))) VirtioNetDesc  rx_desc[128];
    __attribute__((aligned(4096))) VirtioNetAvail rx_avail;
    __attribute__((aligned(4096))) VirtioNetUsed  rx_used;

    __attribute__((aligned(4096))) VirtioNetDesc  tx_desc[128];
    __attribute__((aligned(4096))) VirtioNetAvail tx_avail;
    __attribute__((aligned(4096))) VirtioNetUsed  tx_used;

    uint16_t compute_checksum(uint16_t* addr, int count);
    void send_raw_packet(uint8_t* payload, size_t length);

public:
    NetworkStack();
    void init_virtio_network();
    void process_incoming_packet(uint8_t* buffer, size_t length);
    
    // Éles kapcsolatépítő interfész (Socket-szerű implementáció) [source: 1]
    bool network_connect(uint32_t target_ip, uint16_t target_port, const char* protocol);
    void send_data(const uint8_t* data, size_t length, uint32_t target_ip, uint16_t target_port, const char* protocol);
};

extern NetworkStack net_engine;
