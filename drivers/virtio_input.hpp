#pragma once

#include <stdint.h>

struct VirtioInputEvent {
    uint16_t type;
    uint16_t code;
    int32_t value;
};

namespace virtio_input {

bool init();
bool poll(VirtioInputEvent* event);
int8_t read_byte_nonblocking();

}
