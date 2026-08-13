#pragma once

#include <stdint.h>

namespace blockos::init {

uint64_t monotonic_ms();

void sleep_ms(uint32_t milliseconds);

} // namespace blockos::init
