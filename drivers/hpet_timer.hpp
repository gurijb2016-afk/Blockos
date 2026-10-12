#pragma once

#include <stdint.h>
#include "acpi.hpp"

namespace hpet_timer {

// Bring up the ACPI HPET device while its MMIO mapping is accessible.
// Returns false when firmware does not expose a usable HPET.
bool init(const acpi::Rsdp* rsdp);
bool available();
uint64_t counter_ticks();
uint64_t monotonic_ns();
uint64_t period_femtoseconds();

// Busy-wait for at most max_polls iterations. Returns false if the clock
// does not advance or the deadline is not reached before the poll limit.
bool busy_wait_ns(uint64_t duration_ns, uint64_t max_polls = 100000000ULL);

} // namespace hpet_timer
