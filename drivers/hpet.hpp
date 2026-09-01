#pragma once

#include <stdint.h>
#include <stddef.h>

namespace hpet
{

struct HpetInfo
{
    uint64_t base_address;
    uint64_t period_fs;
    uint8_t  timer_count;
    bool     legacy_replacement;
    bool     initialized;
};

bool init(uint64_t base_address);

bool initialize_from_acpi(
    uint64_t hpet_base_address);

bool is_initialized();

uint64_t read_counter();

uint64_t counter_frequency_hz();

uint64_t counter_period_fs();

uint64_t milliseconds();

uint64_t microseconds();

void sleep_us(uint64_t us);

const HpetInfo& get_info();

}
