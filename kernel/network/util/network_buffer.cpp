#include "network_buffer.hpp"
#include <cstring>

namespace blockos::network {

void NetworkBuffer::clear() {
    size_ = 0;
    std::memset(buf_, 0, sizeof(buf_));
}

} // namespace blockos::network
