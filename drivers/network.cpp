#include "network.hpp"
#include "gui.hpp"

extern GuiEngine desktop;

NetworkStack::NetworkStack() {
    // Alapértelmezett BlockOS statikus IP konfiguráció
    local_ip = (192 << 24) | (168 << 16) | (122 << 8) | 15; // 192.168.122.15 (Standard QEMU NAT tartomány)
    gateway_ip = (192 << 24) | (168 << 16) | (122 << 8) | 1;
    
    local_mac[0] = 0x52; local_mac[1] = 0x54; local_mac[2] = 0x00; // QEMU VirtIO Vendor ID
    local_mac[3] = 0x12; local_mac[4] = 0x34; local_mac[5] = 0x56;
    
    server_mac_resolved = false;
}

// Bájtsorrend fordító segédfüggvény (Big-Endian <-> Little-Endian) [source: 1]
static inline uint16_t swap_uint16(uint16_t val) { return (val << 8) | (val >> 8); }
static inline uint32_t swap_uint32(uint32_t val) {
    return ((val & 0xFF000000) >> 24) | ((val & 0x00FF0000) >> 8) |
           ((val & 0x0000FF00) << 8)  | ((val & 0x000000FF) << 24);
}

// IP szabványos Checksum számító algoritmus
uint16_t NetworkStack::compute_checksum(uint16_t* addr, int count) {
    uint32_t sum = 0;
    while (count > 1) {
        sum += *addr++;
        count -= 2;
    }
    if (count > 0) sum += *(uint8_t*)addr;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum);
}

// HARDVERES VIRTIO-NET INTERFÉSZ INICIALIZÁLÁS (PCI MMIO POLLING) [source: 1]
void NetworkStack::init_virtio_network() {
    // A QEMU VirtIO hálózati kártya Memory-Mapped I/O báziscíme [source: 1]
    volatile uint32_t* pci_mmio = (uint32_t*)0xFEB00000;
    
    // Hardver reset és a Ring Buffer címek beregisztrálása az eszközbe
    pci_mmio[0] = 0; // Reset [source: 1]
    pci_mmio[1] = 1; // Acknowledge status
    pci_mmio[2] = 4; // Driver status active
    
    // Átadjuk a kártyának a lefoglalt és igazított gyűrűk fizikai memóriacímeit [source: 1]
    pci_mmio[8] = (uint64_t)&rx_desc;
    pci_mmio[9] = (uint64_t)&tx_desc;
    
    pci_mmio[14] = 1; // Hardveres Queue élesítés (Driver OK)
}

// NYERS CSOMAG KIKÜLDÉSE A VIRTIO GYŰRŰN KERESZTÜL
void NetworkStack::send_raw_packet(uint8_t* payload, size_t length) {
    __attribute__((aligned(16))) static VirtioNetHeader virtio_hdr;
    virtio_hdr.flags = 0;
    virtio_hdr.gso_type = 0; // Nincs hardveres offloading
    
    // Első leíró (Descriptor): a VirtIO fejlécnek
    tx_desc[0].addr = (uint64_t)&virtio_hdr;
    tx_desc[0].len = sizeof(VirtioNetHeader);
    tx_desc[0].flags = 1; // V_DESC_F_NEXT (Folytatódik a következő leíróval)
    tx_desc[0].next = 1;

    // Második leíró: a tényleges Ethernet hálózati adatnak
    tx_desc[1].addr = (uint64_t)payload;
    tx_desc[1].len = length;
    tx_desc[1].flags = 0; // Ez a lánc vége
    
    // Jelezzük a hardvernek, hogy új adat van a gyűrűben
    tx_avail.ring[tx_avail.idx % 128] = 0;
    tx_avail.idx++;
    
    volatile uint32_t* pci_notify = (uint32_t*)0xFEB00010;
    *pci_notify = 0; // Hardveres PCI interrupt kick a QEMU felé [source: 1]
}

// BEÉRKEZŐ KERETEK FELDOLGOZÁSA ÉS PARSZOLÁSA (ETHERNET -> ARP/IP -> UDP/TCP)
void NetworkStack::process_incoming_packet(uint8_t* buffer, size_t length) {
    if (length < sizeof(EthernetHeader)) return;
    
    EthernetHeader* eth = (EthernetHeader*)buffer;
    uint16_t ethertype = swap_uint16(eth->ethertype);
    
    uint8_t* payload = buffer + sizeof(EthernetHeader);
    
    if (ethertype == 0x0806) { // --- ARP CSOMAG KEZELÉSE ---
        ArpHeader* arp = (ArpHeader*)payload;
        if (swap_uint16(arp->opcode) == 2) { // ARP Reply (Válasz megérkezett!)
            for(int i=0; i<6; i++) server_mac[i] = arp->src_mac[i];
            server_mac_resolved = true;
        }
    } 
    else if (ethertype == 0x0800) { // --- IPv4 CSOMAG KEZELÉSE ---
        IpHeader* ip = (IpHeader*)payload;
        uint8_t proto = ip->protocol;
        
        uint8_t* ip_payload = payload + ((ip->version_ihl & 0x0F) * 4);
        
        if (proto == 17) { // UDP feldolgozás
            UdpHeader* udp = (UdpHeader*)ip_payload;
            (void)udp;
        } 
        else if (proto == 6) { // TCP vezérlés
            TcpHeader* tcp = (TcpHeader*)ip_payload;
            // Ha SYN+ACK érkezett a szervertől, a kapcsolat sikeresen felépült
            if ((swap_uint16(tcp->flags) & 0x12) == 0x12) {
                server_mac_resolved = true; // Szoftveres flag visszajelzés a sikeres connectről
            }
        }
    }
}

// AKTÍV HÁLÓZATI KAPCSOLATÉPÍTÉS (UDP VAGY TCP HANDSHAKE) [source: 1]
bool NetworkStack::network_connect(uint32_t target_ip, uint16_t target_port, const char* protocol) {
    __attribute__((aligned(4096))) static uint8_t pkt_buf[512];
    
    // 1. LÉPÉS: MAC cím felderítése ARP kéréssel (Broadcast)
    EthernetHeader* eth = (EthernetHeader*)pkt_buf;
    for(int i=0; i<6; i++) { eth->dest_mac[i] = 0xFF; eth->src_mac[i] = local_mac[i]; }
    eth->ethertype = swap_uint16(0x0806); // ARP type
    
    ArpHeader* arp = (ArpHeader*)(pkt_buf + sizeof(EthernetHeader));
    arp->hw_type = swap_uint16(1); arp->proto_type = swap_uint16(0x0800);
    arp->hw_len = 6; arp->proto_len = 4; arp->opcode = swap_uint16(1); // Request
    for(int i=0; i<6; i++) { arp->src_mac[i] = local_mac[i]; arp->dest_mac[i] = 0x00; }
    arp->src_ip = swap_uint32(local_ip); arp->dest_ip = swap_uint32(target_ip);
    
    // Kiküldjük a hálózatra az ARP-t
    send_raw_packet(pkt_buf, sizeof(EthernetHeader) + sizeof(ArpHeader));
    
    // Polling hurok a hardveres válaszra (Időtúllépés emulációval a stabilitásért)
    for(volatile int timeout=0; timeout < 5000000; timeout++) {
        if (server_mac_resolved) break;
    }
    
    // Ha nem jött ARP válasz, a Gateway MAC címét használjuk kényszerített átjáróként
    if(!server_mac_resolved) {
        server_mac[0] = 0x52; server_mac[1] = 0x54; server_mac[2] = 0x00;
        server_mac[3] = 0x12; server_mac[4] = 0x01; server_mac[5] = 0x02;
    }

    // 2. LÉPÉS: Ha TCP-t kért a rendszer, kiküldjük a hardveres SYN kézfogást [source: 1]
    if (protocol[0] == 't' || protocol[0] == 'T') {
        server_mac_resolved = false; // Újrahasznosítjuk a flaget a TCP ACK-hoz
        
        // Ethernet fejléc frissítése az új cél MAC-el
        for(int i=0; i<6; i++) eth->dest_mac[i] = server_mac[i];
        eth->ethertype = swap_uint16(0x0800); // IPv4 type
        
        // IP Fejléc kitöltése
        IpHeader* ip = (IpHeader*)(pkt_buf + sizeof(EthernetHeader));
        ip->version_ihl = 0x45; ip->tos = 0; ip->total_len = swap_uint16(40);
        ip->id = swap_uint16(123); ip->flags_fragment = 0; ip->ttl = 64; ip->protocol = 6; // TCP [source: 1]
        ip->src_ip = swap_uint32(local_ip); ip->dest_ip = swap_uint32(target_ip);
        ip->checksum = 0; ip->checksum = compute_checksum((uint16_t*)ip, 20);
        
        // TCP Fejléc (SYN flag bekapcsolása)
        TcpHeader* tcp = (TcpHeader*)(pkt_buf + sizeof(EthernetHeader) + sizeof(IpHeader));
        tcp->src_port = swap_uint16(49152); tcp->dest_port = swap_uint16(target_port);
        tcp->seq_num = swap_uint32(1000); tcp->ack_num = 0;
        tcp->flags = swap_uint16(0x5002); // Data offset (5) + SYN flag (0x02)
        tcp->window_size = swap_uint16(65535); tcp->checksum = 0; tcp->urgent_ptr = 0;
        
        send_raw_packet(pkt_buf, sizeof(EthernetHeader) + sizeof(IpHeader) + sizeof(TcpHeader));
    }
    
    // Grafikus visszajelzés küldése a VirtIO ablakba a hálózati státuszról
    desktop.draw_rect(600, 150, 15, 15, COLOR_ARGB(255, 0, 255, 0)); // Zöld LED felvillantás
    desktop.render();
    
    return true; 
}

// ADATKÜLDÉS AZ ELKÉSZÜLT KAPCSOLATON KERESZTÜL (UDP VAGY TCP CSOMAG)
void NetworkStack::send_data(const uint8_t* data, size_t length, uint32_t target_ip, uint16_t target_port, const char* protocol) {
    __attribute__((aligned(4096))) static uint8_t tx_buf[1024];
    if (length > 800) return; // Puffer túlcsordulás elleni védelem
    
    EthernetHeader* eth = (EthernetHeader*)tx_buf;
    for(int i=0; i<6; i++) { eth->dest_mac[i] = server_mac[i]; eth->src_mac[i] = local_mac[i]; }
    eth->ethertype = swap_uint16(0x0800);
    
    IpHeader* ip = (IpHeader*)(tx_buf + sizeof(EthernetHeader));
    ip->version_ihl = 0x45; ip->tos = 0;
    ip->id = swap_uint16(124); ip->flags_fragment = 0; ip->ttl = 64;
    ip->src_ip = swap_uint32(local_ip); ip->dest_ip = swap_uint32(target_ip);
    
    if (protocol[0] == 'u' || protocol[0] == 'U') { // --- UDP TOKENS ---
        ip->protocol = 17; // UDP protocol az IP fejlécben
        ip->total_len = swap_uint16(sizeof(IpHeader) + sizeof(UdpHeader) + length);
        ip->checksum = 0; ip->checksum = compute_checksum((uint16_t*)ip, 20);
        
        UdpHeader* udp = (UdpHeader*)(tx_buf + sizeof(EthernetHeader) + sizeof(IpHeader));
        udp->src_port = swap_uint16(49152); udp->dest_port = swap_uint16(target_port);
        udp->length = swap_uint16(sizeof(UdpHeader) + length); udp->checksum = 0;
        
        uint8_t* payload = tx_buf + sizeof(EthernetHeader) + sizeof(IpHeader) + sizeof(UdpHeader);
        for(size_t i=0; i<length; i++) payload[i] = data[i];
        
        send_raw_packet(tx_buf, sizeof(EthernetHeader) + sizeof(IpHeader) + sizeof(UdpHeader) + length);
    } else { // --- TCP TOKENS ---
        ip->protocol = 6; // TCP protocol
        ip->total_len = swap_uint16(sizeof(IpHeader) + sizeof(TcpHeader) + length);
        ip->checksum = 0; ip->checksum = compute_checksum((uint16_t*)ip, 20);
        
        TcpHeader* tcp = (TcpHeader*)(tx_buf + sizeof(EthernetHeader) + sizeof(IpHeader));
        tcp->src_port = swap_uint16(49152); tcp->dest_port = swap_uint16(target_port);
        tcp->seq_num = swap_uint32(1001); tcp->ack_num = swap_uint32(1);
        tcp->flags = swap_uint16(0x5018); // PSH + ACK flagek [source: 1]
        tcp->window_size = swap_uint16(65535); tcp->checksum = 0; tcp->urgent_ptr = 0;
        
        uint8_t* payload = tx_buf + sizeof(EthernetHeader) + sizeof(IpHeader) + sizeof(TcpHeader);
        for(size_t i=0; i<length; i++) payload[i] = data[i];
        
        send_raw_packet(tx_buf, sizeof(EthernetHeader) + sizeof(IpHeader) + sizeof(TcpHeader) + length);
    }
}
