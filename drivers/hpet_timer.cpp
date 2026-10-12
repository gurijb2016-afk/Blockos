#include "hpet_timer.hpp"

#include <stddef.h>
#include <stdint.h>

namespace hpet_timer {
namespace {

#pragma pack(push, 1)
struct HpetTable {
    acpi::SdtHeader header;
    uint32_t event_timer_block_id;
    acpi::GenericAddress base_address;
    uint8_t hpet_number;
    uint16_t minimum_tick;
    uint8_t page_protection;
};
#pragma pack(pop)

static constexpr uint64_t HPET_GENERAL_CAPABILITIES = 0x000;
static constexpr uint64_t HPET_GENERAL_CONFIGURATION = 0x010;
static constexpr uint64_t HPET_MAIN_COUNTER = 0x0F0;
static constexpr uint64_t HPET_ENABLE = 1ULL;
static constexpr uint64_t HPET_COUNTER_64BIT = 1ULL << 13;
static constexpr uint64_t FEMTOSECONDS_PER_NANOSECOND = 1000000ULL;

static volatile uint8_t* g_mmio = nullptr;
static uint64_t g_period_fs = 0;
static bool g_counter_64bit = false;
static bool g_ready = false;
static volatile uint32_t g_counter_lock = 0;
static uint32_t g_last_low = 0;
static uint64_t g_high = 0;

static inline uint64_t read64(uint64_t offset)
{
    return *reinterpret_cast<volatile uint64_t*>(
        const_cast<uint8_t*>(g_mmio) + offset);
}

static inline uint32_t read32(uint64_t offset)
{
    return *reinterpret_cast<volatile uint32_t*>(
        const_cast<uint8_t*>(g_mmio) + offset);
}

static inline void write64(uint64_t offset, uint64_t value)
{
    *reinterpret_cast<volatile uint64_t*>(
        const_cast<uint8_t*>(g_mmio) + offset) = value;
}

static void lock_counter()
{
    while (__atomic_test_and_set(&g_counter_lock, __ATOMIC_ACQUIRE))
        __asm__ volatile("pause");
}

static void unlock_counter()
{
    __atomic_clear(&g_counter_lock, __ATOMIC_RELEASE);
}

} // namespace

bool init(const acpi::Rsdp* rsdp)
{
    g_ready = false;
    g_mmio = nullptr;
    g_period_fs = 0;
    g_counter_64bit = false;
    g_last_low = 0;
    g_high = 0;

    if (!rsdp)
        return false;

    const acpi::SdtHeader* header = acpi::find_table(rsdp, "HPET");
    if (!header || header->length < sizeof(HpetTable))
        return false;
    if (!acpi::checksum_ok(header, header->length))
        return false;

    const HpetTable* table = reinterpret_cast<const HpetTable*>(header);
    // HPET's GAS address_space_id 0 is System Memory. I/O-space HPETs are
    // deliberately rejected rather than being dereferenced as MMIO.
    if (table->base_address.address_space != 0 ||
        table->base_address.address == 0)
        return false;

    g_mmio = reinterpret_cast<volatile uint8_t*>(
        static_cast<uintptr_t>(table->base_address.address));

    const uint64_t capabilities = read64(HPET_GENERAL_CAPABILITIES);
    g_period_fs = capabilities >> 32;
    g_counter_64bit = (capabilities & HPET_COUNTER_64BIT) != 0;

    // ACPI specifies a period in the 1 ns .. 100 us range. Reject zero and
    // implausible values rather than creating a clock that never progresses.
    if (g_period_fs < 1000000ULL || g_period_fs > 100000000000ULL) {
        g_mmio = nullptr;
        g_period_fs = 0;
        return false;
    }

    uint64_t config = read64(HPET_GENERAL_CONFIGURATION);
    if ((config & HPET_ENABLE) == 0) {
        write64(HPET_GENERAL_CONFIGURATION, config | HPET_ENABLE);
        config = read64(HPET_GENERAL_CONFIGURATION);
    }

    if ((config & HPET_ENABLE) == 0) {
        g_mmio = nullptr;
        g_period_fs = 0;
        return false;
    }

    g_ready = true;
    return true;
}

bool available()
{
    return g_ready;
}

uint64_t counter_ticks()
{
    if (!g_ready || !g_mmio)
        return 0;

    if (g_counter_64bit)
        return read64(HPET_MAIN_COUNTER);

    // Extend a 32-bit HPET counter to 64 bits. A short lock keeps simultaneous
    // callers from counting the same wrap twice on a multi-CPU machine.
    lock_counter();
    const uint32_t low = read32(HPET_MAIN_COUNTER);
    if (low < g_last_low)
        g_high += (1ULL << 32);
    g_last_low = low;
    const uint64_t value = g_high | low;
    unlock_counter();
    return value;
}

uint64_t monotonic_ns()
{
    if (!g_ready || !g_period_fs)
        return 0;

    const uint64_t ticks = counter_ticks();
    // Equivalent to ticks * period_fs / 1,000,000, but the split reduces
    // intermediate overflow while the final nanosecond result fits uint64_t.
    return (ticks / FEMTOSECONDS_PER_NANOSECOND) * g_period_fs +
           ((ticks % FEMTOSECONDS_PER_NANOSECOND) * g_period_fs) /
               FEMTOSECONDS_PER_NANOSECOND;
}

uint64_t period_femtoseconds()
{
    return g_ready ? g_period_fs : 0;
}

bool busy_wait_ns(uint64_t duration_ns, uint64_t max_polls)
{
    if (!g_ready || max_polls == 0)
        return false;
    if (duration_ns == 0)
        return true;

    const uint64_t start = monotonic_ns();
    uint64_t previous = start;
    uint32_t stagnant = 0;

    for (uint64_t i = 0; i < max_polls; ++i) {
        const uint64_t now = monotonic_ns();
        if (now - start >= duration_ns)
            return true;
        if (now == previous) {
            if (++stagnant >= 1000000U)
                return false;
        } else {
            previous = now;
            stagnant = 0;
        }
        __asm__ volatile("pause");
    }
    return false;
}

} // namespace hpet_timer
