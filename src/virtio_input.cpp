#include "virtio_input.hpp"
#include <cstdint>

// Stub implementation: placeholder for future virtio-input support.
// Currently returns no data; design exists so kernel can switch to virtio input later.

void VirtioInput::init() {
    // TODO: implement virtio device detection and initialization
}

int8_t VirtioInput::read_byte_nonblocking() {
    return INT8_MIN; // no data
}
