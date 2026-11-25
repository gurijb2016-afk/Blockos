#include "nvme.hpp"
#include "pci.hpp"
#include <stdint.h>
#include <stddef.h>

namespace nvme {
namespace {

constexpr uint32_t REG_CAP  = 0x00;
constexpr uint32_t REG_CC   = 0x14;
constexpr uint32_t REG_CSTS = 0x1C;
constexpr uint32_t REG_AQA  = 0x24;
constexpr uint32_t REG_ASQ  = 0x28;
constexpr uint32_t REG_ACQ  = 0x30;
constexpr uint32_t REG_DB   = 0x1000;

constexpr uint16_t ADMIN_Q_SIZE = 64;
constexpr uint16_t MAX_IO_Q_SIZE = 128;
constexpr size_t PAGE_SIZE = 4096;
constexpr size_t PRP_LIST_ENTRIES = PAGE_SIZE / sizeof(uint64_t);

struct Cmd {
    uint32_t cdw0;
    uint32_t nsid;
    uint32_t rsv2[2]; // CDW2..CDW3 (8 bytes)
    uint64_t mptr;
    uint64_t prp1;
    uint64_t prp2;
    uint32_t cdw10;
    uint32_t cdw11;
    uint32_t cdw12;
    uint32_t cdw13;
    uint32_t cdw14;
    uint32_t cdw15;
} __attribute__((packed, aligned(4)));

struct Cpl {
    uint32_t dw0;
    uint32_t dw1;
    uint32_t dw2;
    uint32_t dw3;
} __attribute__((packed));

static_assert(sizeof(Cmd) == 64, "NVMe SQE must be 64 bytes");
static_assert(sizeof(Cpl) == 16, "NVMe CQE must be 16 bytes");

struct QueuePair {
    uint16_t qid;
    uint16_t size;
    uint16_t sq_tail;
    uint16_t cq_head;
    uint8_t  phase;
    Cmd*     sq;
    Cpl*     cq;
};

static volatile uint8_t* bar = nullptr;
static uint64_t cap = 0;
static uint32_t db_stride = 4;
static bool ready = false;
static uint16_t next_cid = 1;

alignas(PAGE_SIZE) static Cmd asq[ADMIN_Q_SIZE];
alignas(PAGE_SIZE) static Cpl acq[ADMIN_Q_SIZE];
alignas(PAGE_SIZE) static Cmd ioq[MAX_IO_Q_SIZE];
alignas(PAGE_SIZE) static Cpl ioq_cq[MAX_IO_Q_SIZE];
alignas(PAGE_SIZE) static uint8_t identify_buf[PAGE_SIZE];
alignas(PAGE_SIZE) static uint8_t identify_ns_buf[PAGE_SIZE];
alignas(PAGE_SIZE) static uint64_t prp_list[PRP_LIST_ENTRIES];

static QueuePair admin_q{};
static QueuePair io_q{};

static inline void zero_mem(void* p, size_t n) {
    uint8_t* b = reinterpret_cast<uint8_t*>(p);
    for (size_t i = 0; i < n; ++i) b[i] = 0;
}

static inline void wr32(uint32_t off, uint32_t v) {
    *reinterpret_cast<volatile uint32_t*>(bar + off) = v;
}

static inline void wr64(uint32_t off, uint64_t v) {
    *reinterpret_cast<volatile uint32_t*>(bar + off) = static_cast<uint32_t>(v & 0xFFFFFFFFu);
    *reinterpret_cast<volatile uint32_t*>(bar + off + 4) = static_cast<uint32_t>(v >> 32);
}

static inline uint32_t rd32(uint32_t off) {
    return *reinterpret_cast<volatile uint32_t*>(bar + off);
}

static inline uint64_t rd64(uint32_t off) {
    uint64_t lo = rd32(off);
    uint64_t hi = rd32(off + 4);
    return lo | (hi << 32);
}

static inline void spin_pause() {
    for (uint32_t i = 0; i < 64; ++i) {
        __asm__ volatile("pause");
    }
}

static bool find() {
    for (uint16_t b = 0; b < 256; ++b) {
        for (uint8_t s = 0; s < 32; ++s) {
            for (uint8_t f = 0; f < 8; ++f) {
                if (pci_cfg_read16(static_cast<uint8_t>(b), s, f, 0) == 0xFFFF)
                    continue;
                if (pci_cfg_read8(static_cast<uint8_t>(b), s, f, 0x0B) == 0x01 &&
                    pci_cfg_read8(static_cast<uint8_t>(b), s, f, 0x0A) == 0x08 &&
                    pci_cfg_read8(static_cast<uint8_t>(b), s, f, 0x09) == 0x02) {
                    uint64_t x = pci_read_bar(static_cast<uint8_t>(b), s, f, 0);
                    if (x != 0) {
                        // Enable PCI Memory Space (bit 1) and Bus Master (bit 2)
                        uint16_t cmd = pci_cfg_read16(static_cast<uint8_t>(b), s, f, 0x04);
                        cmd |= static_cast<uint16_t>((1u << 1) | (1u << 2));
                        pci_cfg_write16(static_cast<uint8_t>(b), s, f, 0x04, cmd);
                        bar = reinterpret_cast<volatile uint8_t*>(static_cast<uintptr_t>(x));
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

static bool submit(QueuePair& q, Cmd& c) {
    uint16_t id = next_cid++;
    if (next_cid == 0) next_cid = 1;
    c.cdw0 = (static_cast<uint32_t>(id) << 16) | (c.cdw0 & 0xFFFFu);

    q.sq[q.sq_tail] = c;
    q.sq_tail = static_cast<uint16_t>((q.sq_tail + 1u) % q.size);

    __asm__ volatile("sfence" ::: "memory");
    const uint32_t sq_db = REG_DB + db_stride * (2u * q.qid);
    const uint32_t cq_db = sq_db + db_stride;
    wr32(sq_db, q.sq_tail);

    for (uint32_t t = 0; t < 5000000; ++t) {
        volatile Cpl* cp = &q.cq[q.cq_head];
        uint32_t dw3 = cp->dw3;
        __asm__ volatile("lfence" ::: "memory");
        uint16_t status = static_cast<uint16_t>(dw3 >> 16);
        if ((status & 1u) == q.phase) {
            uint16_t cpl_cid = static_cast<uint16_t>(dw3 & 0xFFFFu);
            uint16_t sc = static_cast<uint16_t>((status >> 1) & 0xFFu);
            uint16_t sct = static_cast<uint16_t>((status >> 9) & 0x7u);

            q.cq_head = static_cast<uint16_t>((q.cq_head + 1u) % q.size);
            if (q.cq_head == 0) q.phase ^= 1u;
            wr32(cq_db, q.cq_head);

            if (cpl_cid == id) {
                return sc == 0 && sct == 0;
            }
        }
        spin_pause();
    }
    return false;
}

static bool setup() {
    cap = rd64(REG_CAP);
    db_stride = 4u << ((cap >> 32) & 0xFu);

    // Disable controller before configuring Admin Queue registers
    uint32_t cc = rd32(REG_CC);
    if (cc & 1u) {
        cc &= ~1u;
        wr32(REG_CC, cc);
    }
    bool disabled = false;
    for (uint32_t i = 0; i < 1000000; ++i) {
        if ((rd32(REG_CSTS) & 1u) == 0) {
            disabled = true;
            break;
        }
        spin_pause();
    }
    if (!disabled) return false;

    zero_mem(asq, sizeof(asq));
    zero_mem(acq, sizeof(acq));
    zero_mem(ioq, sizeof(ioq));
    zero_mem(ioq_cq, sizeof(ioq_cq));

    admin_q = {0, ADMIN_Q_SIZE, 0, 0, 1, asq, acq};

    wr32(REG_AQA, (static_cast<uint32_t>(ADMIN_Q_SIZE - 1u) << 16) | (ADMIN_Q_SIZE - 1u));
    wr64(REG_ASQ, static_cast<uint64_t>(reinterpret_cast<uintptr_t>(asq)));
    wr64(REG_ACQ, static_cast<uint64_t>(reinterpret_cast<uintptr_t>(acq)));

    // Enable controller: EN=1, CSS=0 (NVM), MPS=0 (4 KiB), IOSQES=6 (64B), IOCQES=4 (16B)
    cc = 1u | (6u << 16) | (4u << 20);
    wr32(REG_CC, cc);

    for (uint32_t i = 0; i < 1000000; ++i) {
        uint32_t csts = rd32(REG_CSTS);
        if (csts & (1u << 1)) return false; // Controller Fatal Status
        if (csts & 1u) return true;
        spin_pause();
    }
    return false;
}

static bool create_io() {
    uint32_t mqes = static_cast<uint32_t>(cap & 0xFFFFu) + 1u;
    uint16_t io_size = mqes < MAX_IO_Q_SIZE ? static_cast<uint16_t>(mqes) : MAX_IO_Q_SIZE;
    if (io_size < 2) return false;

    // Create I/O Completion Queue (QID=1, PC=1, IEN=0)
    Cmd c{};
    c.cdw0 = 0x05;
    c.prp1 = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ioq_cq));
    c.cdw10 = (static_cast<uint32_t>(io_size - 1u) << 16) | 1u;
    c.cdw11 = 1u;
    if (!submit(admin_q, c)) return false;

    // Create I/O Submission Queue (QID=1, CQID=1 in bits 31:16, PC=1)
    c = {};
    c.cdw0 = 0x01;
    c.prp1 = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ioq));
    c.cdw10 = (static_cast<uint32_t>(io_size - 1u) << 16) | 1u;
    c.cdw11 = (1u << 16) | 1u;
    if (!submit(admin_q, c)) return false;

    io_q = {1, io_size, 0, 0, 1, ioq, ioq_cq};
    return true;
}

static bool identify_ctrl() {
    zero_mem(identify_buf, sizeof(identify_buf));
    Cmd c{};
    c.cdw0 = 0x06;
    c.prp1 = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(identify_buf));
    c.cdw10 = 1u; // CNS = 1 (Identify Controller)
    return submit(admin_q, c);
}

static bool setup_prp(Cmd& c, uint64_t phys_addr, size_t total_bytes) {
    c.prp1 = phys_addr;
    c.prp2 = 0;

    size_t page_off = static_cast<size_t>(phys_addr & (PAGE_SIZE - 1));
    size_t first_chunk = PAGE_SIZE - page_off;
    if (total_bytes <= first_chunk) {
        return true;
    }

    size_t rem = total_bytes - first_chunk;
    uint64_t next_page = (phys_addr & ~static_cast<uint64_t>(PAGE_SIZE - 1)) + PAGE_SIZE;

    if (rem <= PAGE_SIZE) {
        c.prp2 = next_page;
        return true;
    }

    size_t num_pages = (rem + PAGE_SIZE - 1) / PAGE_SIZE;
    if (num_pages > PRP_LIST_ENTRIES) {
        return false;
    }

    for (size_t i = 0; i < num_pages; ++i) {
        prp_list[i] = next_page + static_cast<uint64_t>(i) * PAGE_SIZE;
    }
    c.prp2 = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(prp_list));
    return true;
}

static bool rw_chunk(bool wr, uint32_t nsid, uint64_t lba, uint16_t blocks,
                     const void* buf, size_t block_size) {
    size_t total_bytes = static_cast<size_t>(blocks) * block_size;
    Cmd c{};
    c.cdw0 = wr ? 1u : 2u;
    c.nsid = nsid;
    if (!setup_prp(c, static_cast<uint64_t>(reinterpret_cast<uintptr_t>(buf)), total_bytes)) {
        return false;
    }
    c.cdw10 = static_cast<uint32_t>(lba & 0xFFFFFFFFu);
    c.cdw11 = static_cast<uint32_t>(lba >> 32);
    c.cdw12 = static_cast<uint32_t>(blocks - 1u) & 0xFFFFu;
    return submit(io_q, c);
}

static bool rw(bool wr, uint32_t nsid, uint64_t lba, uint16_t blocks,
               const void* buf, size_t bs) {
    if (!ready || !buf || !blocks || bs == 0) return false;
    // Cap each single command so it always fits inside our single-page PRP list (<= 2 MiB)
    const size_t max_bytes = (PRP_LIST_ENTRIES - 1) * PAGE_SIZE;
    uint16_t max_blocks = static_cast<uint16_t>(max_bytes / bs);
    if (max_blocks == 0) return false;

    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(buf);
    uint16_t remaining = blocks;
    uint64_t cur_lba = lba;
    while (remaining > 0) {
        uint16_t chunk = remaining > max_blocks ? max_blocks : remaining;
        if (!rw_chunk(wr, nsid, cur_lba, chunk, ptr, bs)) return false;
        ptr += static_cast<size_t>(chunk) * bs;
        cur_lba += chunk;
        remaining = static_cast<uint16_t>(remaining - chunk);
    }
    return true;
}

} // namespace

bool init() {
    if (ready) return true;
    if (!find() || !setup()) return false;
    if (!identify_ctrl()) return false;
    if (!create_io()) return false;
    ready = true;
    return true;
}

bool available() { return ready; }

bool identify(uint8_t out_model[40], uint32_t& nsc) {
    if (!ready || !out_model) return false;
    for (int i = 0; i < 40; ++i) out_model[i] = identify_buf[24 + i];
    nsc = *reinterpret_cast<uint32_t*>(identify_buf + 516);
    return true;
}

bool identify_namespace(uint32_t nsid, uint64_t& block_count, uint32_t& block_size) {
    if (!ready || nsid == 0) return false;
    zero_mem(identify_ns_buf, sizeof(identify_ns_buf));
    Cmd c{};
    c.cdw0 = 0x06;
    c.nsid = nsid;
    c.prp1 = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(identify_ns_buf));
    c.cdw10 = 0u; // CNS = 0 (Identify Namespace)
    if (!submit(admin_q, c)) return false;

    block_count = *reinterpret_cast<uint64_t*>(identify_ns_buf + 0);
    uint8_t flbas = identify_ns_buf[26] & 0x0Fu;
    uint32_t lbaf = *reinterpret_cast<uint32_t*>(identify_ns_buf + 128 + static_cast<size_t>(flbas) * 4);
    uint8_t lbads = static_cast<uint8_t>((lbaf >> 16) & 0xFFu);
    if (lbads < 9 || lbads > 16) return false;
    block_size = 1u << lbads;
    return true;
}

bool read(uint32_t n, uint64_t l, uint16_t b, void* x, size_t s) {
    return rw(false, n, l, b, x, s);
}

bool write(uint32_t n, uint64_t l, uint16_t b, const void* x, size_t s) {
    return rw(true, n, l, b, x, s);
}

} // namespace nvme
