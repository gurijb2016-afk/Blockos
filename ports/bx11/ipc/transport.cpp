#include "transport.hpp"
#include <cstring>

namespace blockos::bx11::ipc {

bool RingChannel::send(const void *data, size_t size)
{
    if (!data || size > Capacity - size_) return false;
    const auto *src = static_cast<const uint8_t *>(data);
    for (size_t i = 0; i < size; ++i) {
        buffer_[write_] = src[i];
        write_ = (write_ + 1) % Capacity;
    }
    size_ += size;
    return true;
}

bool RingChannel::recv(void *data, size_t size)
{
    if (!data || size > size_) return false;
    auto *dst = static_cast<uint8_t *>(data);
    for (size_t i = 0; i < size; ++i) {
        dst[i] = buffer_[read_];
        read_ = (read_ + 1) % Capacity;
    }
    size_ -= size;
    return true;
}

}
