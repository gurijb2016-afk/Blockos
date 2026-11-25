#include "ahci.hpp"
#include "pci.hpp"
#include <stdint.h>
#include <stddef.h>

namespace ahci {
namespace {

constexpr uint32_t HBA_GHC_AE   = 1u << 31;
constexpr uint32_t PORT_CMD_ST  = 1u << 0;
constexpr uint32_t PORT_CMD_FRE = 1u << 4;
constexpr uint32_t PORT_CMD_FR  = 1u << 14;
constexpr uint32_t PORT_CMD_CR  = 1u << 15;
constexpr uint32_t PORT_IS_TFES = 1u << 30;
constexpr uint32_t PORT_IS_HBFS = 1u << 29;
constexpr uint32_t PORT_IS_HBDS = 1u << 28;
constexpr uint32_t PORT_IS_IFS  = 1u << 27;
constexpr uint32_t PORT_IS_ERR  = PORT_IS_TFES | PORT_IS_HBFS | PORT_IS_HBDS | PORT_IS_IFS;

constexpr uint32_t SATA_SIG_ATA = 0x00000101u;
constexpr uint32_t MAX_PRDT_BYTES = 4u * 1024u * 1024u; // 4 MiB per PRDT entry (DBC max = 0x3FFFFF)

struct HbaPort {
    volatile uint32_t clb, clbu, fb, fbu, is, ie, cmd, rsv0;
    volatile uint32_t tfd, sig, ssts, sctl, serr, sact, ci, sntf, fbs, devslp;
    uint32_t rsv1[10];
    uint32_t vendor[4];
};

struct Hba {
    volatile uint32_t cap, ghc, is, pi, vs, ccc_ctl, ccc_pts, em_loc, em_ctl, cap2, bohc;
    uint8_t rsv[0xA0 - 0x2C];
    uint8_t ports_raw[32 * 0x80];
};

// AHCI 1.3.1 Section 4.2.2 Command Header (32 bytes)
struct CmdHeader {
    uint8_t  flags0; // bits 4:0 = CFL (in DWORDs), bit 5 = A, bit 6 = W, bit 7 = P
    uint8_t  flags1; // bit 0 = R, bit 1 = B, bit 2 = C, bits 7:4 = PMP
    uint16_t prdtl;  // PRDT entry count
    volatile uint32_t prdbc;
    uint64_t ctba;
    uint32_t rsv[4];
} __attribute__((packed));

struct Prdt {
    uint64_t dba;
    uint32_t rsv;
    uint32_t dbc_i; // bits 21:0 = byte count - 1, bit 31 = I
} __attribute__((packed));

struct CmdTable {
    uint8_t cfis[64];
    uint8_t acmd[16];
    uint8_t rsv[48];
    Prdt    prdt[8];
} __attribute__((packed, aligned(128)));

static_assert(sizeof(CmdHeader) == 32, "AHCI CmdHeader must be 32 bytes");
static_assert(sizeof(Prdt) == 16, "AHCI Prdt must be 16 bytes");

struct ReceivedFis {
    uint8_t bytes[256];
} __attribute__((aligned(256)));

static Hba* hba = nullptr;
static bool ready = false;
static uint32_t max_slots = 32;
static PortInfo info[32]{};

alignas(1024) static CmdHeader headers[32][32];
alignas(256)  static ReceivedFis fis_bufs[32];
alignas(128)  static CmdTable tables[32][32];

static inline void zero_mem(void* p, size_t n) {
    uint8_t* b = reinterpret_cast<uint8_t*>(p);
    for (size_t i = 0; i < n; ++i) b[i] = 0;
}

static inline void delay() {
    for (uint32_t i = 0; i < 64; ++i) {
        __asm__ volatile("pause");
    }
}

static inline volatile HbaPort* port_regs(uint8_t port) {
    return reinterpret_cast<volatile HbaPort*>(
        reinterpret_cast<uint8_t*>(hba) + 0x100u + static_cast<size_t>(port) * 0x80u);
}

static bool wait_ready(volatile HbaPort* p) {
    for (uint32_t i = 0; i < 1000000; ++i) {
        if ((p->tfd & 0x88u) == 0) return true; // BSY=0, DRQ=0
        delay();
    }
    return false;
}

static bool stop_port(volatile HbaPort* p) {
    p->cmd &= ~PORT_CMD_ST;
    bool cr_cleared = false;
    for (uint32_t i = 0; i < 1000000; ++i) {
        if ((p->cmd & PORT_CMD_CR) == 0) {
            cr_cleared = true;
            break;
        }
        delay();
    }
    if (!cr_cleared) return false;

    p->cmd &= ~PORT_CMD_FRE;
    for (uint32_t i = 0; i < 1000000; ++i) {
        if ((p->cmd & PORT_CMD_FR) == 0) return true;
        delay();
    }
    return false;
}

static bool start_port(volatile HbaPort* p) {
    for (uint32_t i = 0; i < 1000000; ++i) {
        if ((p->cmd & PORT_CMD_CR) == 0) break;
        delay();
    }
    if (p->cmd & PORT_CMD_CR) return false;
    p->cmd |= PORT_CMD_FRE;
    p->cmd |= PORT_CMD_ST;
    return true;
}

static bool scan() {
    for (uint16_t b = 0; b < 256; ++b) {
        for (uint8_t s = 0; s < 32; ++s) {
            for (uint8_t f = 0; f < 8; ++f) {
                if (pci_cfg_read16(static_cast<uint8_t>(b), s, f, 0) == 0xFFFF)
                    continue;
                uint8_t cls = pci_cfg_read8(static_cast<uint8_t>(b), s, f, 0x0B);
                uint8_t sub = pci_cfg_read8(static_cast<uint8_t>(b), s, f, 0x0A);
                uint8_t pi  = pci_cfg_read8(static_cast<uint8_t>(b), s, f, 0x09);
                if (cls == 0x01 && sub == 0x06 && pi == 0x01) {
                    uint64_t bar = pci_read_bar(static_cast<uint8_t>(b), s, f, 5);
                    if (bar != 0) {
                        // Enable PCI Memory Space (bit 1) and Bus Master (bit 2)
                        uint16_t cmd = pci_cfg_read16(static_cast<uint8_t>(b), s, f, 0x04);
                        cmd |= static_cast<uint16_t>((1u << 1) | (1u << 2));
                        pci_cfg_write16(static_cast<uint8_t>(b), s, f, 0x04, cmd);
                        hba = reinterpret_cast<Hba*>(static_cast<uintptr_t>(bar));
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

static bool prepare(uint8_t port) {
    if (!hba || port >= 32 || !(hba->pi & (1u << port))) return false;
    volatile HbaPort* p = port_regs(port);
    if (!stop_port(p)) return false;

    zero_mem(&headers[port][0], sizeof(headers[port]));
    zero_mem(&fis_bufs[port], sizeof(fis_bufs[port]));
    zero_mem(&tables[port][0], sizeof(tables[port]));

    uint64_t clb = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&headers[port][0]));
    p->clb  = static_cast<uint32_t>(clb & 0xFFFFFFFFu);
    p->clbu = static_cast<uint32_t>(clb >> 32);

    uint64_t fb = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&fis_bufs[port]));
    p->fb  = static_cast<uint32_t>(fb & 0xFFFFFFFFu);
    p->fbu = static_cast<uint32_t>(fb >> 32);

    for (unsigned i = 0; i < 32; ++i) {
        headers[port][i].ctba = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&tables[port][i]));
    }

    p->serr = 0xFFFFFFFFu;
    p->is   = 0xFFFFFFFFu;

    if (!start_port(p)) return false;
    return wait_ready(p);
}

static bool xfer(uint8_t port, uint64_t lba, uint16_t n, void* buf, bool writeop) {
    if (!ready || !buf || !n || port >= 32 || !info[port].disk) return false;
    volatile HbaPort* p = port_regs(port);
    if (!wait_ready(p)) return false;

    uint32_t busy = p->sact | p->ci;
    unsigned slot = 0;
    while (slot < max_slots && (busy & (1u << slot))) ++slot;
    if (slot >= max_slots) return false;

    uint32_t total_bytes = static_cast<uint32_t>(n) * 512u;
    uint16_t prdt_count = static_cast<uint16_t>((total_bytes + MAX_PRDT_BYTES - 1u) / MAX_PRDT_BYTES);
    if (prdt_count == 0 || prdt_count > 8) return false;

    CmdHeader& ch = headers[port][slot];
    CmdTable&  ct = tables[port][slot];
    zero_mem(&ct, sizeof(ct));

    // CFL = 5 DWORDs (20-byte Reg H2D FIS), W = bit 6 for writes
    ch.flags0 = static_cast<uint8_t>(5u | (writeop ? (1u << 6) : 0u));
    ch.flags1 = 0;
    ch.prdtl  = prdt_count;
    ch.prdbc  = 0;

    uint32_t rem = total_bytes;
    uint8_t* ptr = reinterpret_cast<uint8_t*>(buf);
    for (uint16_t i = 0; i < prdt_count; ++i) {
        uint32_t chunk = rem > MAX_PRDT_BYTES ? MAX_PRDT_BYTES : rem;
        ct.prdt[i].dba   = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ptr));
        ct.prdt[i].rsv   = 0;
        ct.prdt[i].dbc_i = chunk - 1u;
        ptr += chunk;
        rem -= chunk;
    }

    // SATA Register Host-to-Device FIS (Type 0x27)
    uint8_t* f = ct.cfis;
    f[0]  = 0x27;                       // FIS_TYPE_REG_H2D
    f[1]  = 0x80;                       // Command bit = 1
    f[2]  = writeop ? 0x35 : 0x25;      // WRITE DMA EXT / READ DMA EXT
    f[3]  = 0x00;                       // Feature 7:0
    f[4]  = static_cast<uint8_t>((lba >>  0) & 0xFFu); // LBA 7:0
    f[5]  = static_cast<uint8_t>((lba >>  8) & 0xFFu); // LBA 15:8
    f[6]  = static_cast<uint8_t>((lba >> 16) & 0xFFu); // LBA 23:16
    f[7]  = 0x40;                       // Device: LBA mode (bit 6)
    f[8]  = static_cast<uint8_t>((lba >> 24) & 0xFFu); // LBA 31:24
    f[9]  = static_cast<uint8_t>((lba >> 32) & 0xFFu); // LBA 39:32
    f[10] = static_cast<uint8_t>((lba >> 40) & 0xFFu); // LBA 47:40
    f[11] = 0x00;                       // Feature 15:8
    f[12] = static_cast<uint8_t>(n & 0xFFu);           // Count 7:0
    f[13] = static_cast<uint8_t>((n >> 8) & 0xFFu);    // Count 15:8

    p->is = 0xFFFFFFFFu;
    __asm__ volatile("sfence" ::: "memory");
    p->ci = (1u << slot);

    for (uint32_t t = 0; t < 5000000; ++t) {
        if ((p->ci & (1u << slot)) == 0) {
            return (p->is & PORT_IS_ERR) == 0 && (p->tfd & 0x01u) == 0;
        }
        if (p->is & PORT_IS_ERR) break;
        delay();
    }
    return false;
}

} // namespace

bool init() {
    if (ready) return true;
    if (!scan()) return false;

    hba->ghc |= HBA_GHC_AE;
    max_slots = ((hba->cap >> 8) & 0x1Fu) + 1u;
    if (max_slots > 32) max_slots = 32;

    uint32_t pi = hba->pi;
    for (uint8_t p = 0; p < 32; ++p) {
        if (pi & (1u << p)) {
            volatile HbaPort* hp = port_regs(p);
            uint32_t s = hp->ssts;
            bool det = (s & 0xFu) == 3u;
            bool ipm = ((s >> 8) & 0xFu) == 1u;
            bool is_disk = det && ipm && hp->sig == SATA_SIG_ATA;
            info[p] = {reinterpret_cast<uintptr_t>(hp), p, true, is_disk};
            if (is_disk) {
                if (!prepare(p)) {
                    info[p].disk = false;
                }
            }
        }
    }
    ready = true;
    return true;
}

bool available() { return ready; }

size_t ports(PortInfo* out, size_t cap) {
    size_t n = 0;
    for (unsigned i = 0; i < 32 && n < cap; ++i) {
        if (info[i].implemented) out[n++] = info[i];
    }
    return n;
}

bool read(uint8_t p, uint64_t l, uint16_t n, void* b) {
    return xfer(p, l, n, b, false);
}

bool write(uint8_t p, uint64_t l, uint16_t n, const void* b) {
    return xfer(p, l, n, const_cast<void*>(b), true);
}

} // namespace ahci
