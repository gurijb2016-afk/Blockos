#include "smp.hpp"

namespace smp {
namespace {

constexpr uint32_t LAPIC_EOI     = 0x0B0;
constexpr uint32_t LAPIC_ESR     = 0x280;
constexpr uint32_t LAPIC_ICR_LO  = 0x300;
constexpr uint32_t LAPIC_ICR_HI  = 0x310;
constexpr uint32_t ICR_DELIV_PENDING = 1u << 12;

static Cpu cpus[64]{};
static size_t count = 0;
static volatile uint32_t* lapic =
    reinterpret_cast<volatile uint32_t*>(static_cast<uintptr_t>(0xFEE00000u));

static inline uint32_t mmio_read(uint32_t reg) {
    return lapic[reg / 4];
}

static inline void mmio_write(uint32_t reg, uint32_t val) {
    lapic[reg / 4] = val;
}

static inline void delay(unsigned loops = 100000) {
    for (unsigned i = 0; i < loops; ++i) {
        __asm__ volatile("pause");
    }
}

static void wait_icr_idle() {
    for (uint32_t i = 0; i < 1000000; ++i) {
        if ((mmio_read(LAPIC_ICR_LO) & ICR_DELIV_PENDING) == 0) return;
        __asm__ volatile("pause");
    }
}

static void send_ipi(uint32_t apic_id, uint32_t icr_lo) {
    wait_icr_idle();
    mmio_write(LAPIC_ESR, 0);
    // xAPIC ICR High expects the 8-bit destination APIC ID in bits 31:24.
    mmio_write(LAPIC_ICR_HI, (apic_id & 0xFFu) << 24);
    mmio_write(LAPIC_ICR_LO, icr_lo);
    wait_icr_idle();
}

} // namespace

void init(const uint32_t* ids, size_t n) {
    count = n > 64 ? 64 : n;
    for (size_t i = 0; i < count; ++i) {
        cpus[i].apic_id = ids[i];
        cpus[i].index = static_cast<uint32_t>(i);
        cpus[i].started = (i == 0) ? 1u : 0u;
    }
}

size_t cpu_count() { return count; }

const Cpu* cpu(size_t i) { return i < count ? &cpus[i] : nullptr; }

void apic_eoi() { mmio_write(LAPIC_EOI, 0); }

void apic_send_init(uint32_t id) {
    // INIT IPI assert (level=1, trigger=level, delivery=INIT)
    send_ipi(id, 0x0000C500u);
    delay(200000);
    // INIT IPI de-assert (level=0, trigger=level, delivery=INIT)
    send_ipi(id, 0x00008500u);
    delay(200000);
}

void apic_send_sipi(uint32_t id, uint8_t v) {
    // Startup IPI (level=1, trigger=edge, delivery=SIPI)
    send_ipi(id, 0x00004600u | static_cast<uint32_t>(v));
    delay(50000);
}

bool start_secondary(uint32_t id, void (*entry)()) {
    if (!entry) return false;
    for (size_t i = 0; i < count; ++i) {
        if (cpus[i].apic_id == id) {
            cpus[i].entry = entry;
            cpus[i].started = 0;
            apic_send_init(id);
            apic_send_sipi(id, 0x08);
            apic_send_sipi(id, 0x08);
            return true;
        }
    }
    return false;
}

} // namespace smp
