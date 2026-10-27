#pragma once

#include <stdint.h>
#include <stddef.h>

namespace virtio_rng {

bool init();
bool is_available();
bool get_random_bytes(void* buffer, size_t length);
uint32_t get_random_u32();

} // namespace virtio_rng
