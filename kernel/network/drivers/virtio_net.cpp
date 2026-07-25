#include "virtio_net.hpp"

namespace blockos::network {

bool VirtioNetAdapter::bring_up() {
    up_ = true;
    return true;
}

bool VirtioNetAdapter::bring_down() {
    up_ = false;
    return true;
}

bool VirtioNetAdapter::send_frame(const void* data, std::size_t len) {
    (void)data;
    (void)len;
    return up_;
}

std::size_t VirtioNetAdapter::poll_frame(void* out, std::size_t max) {
    (void)out;
    (void)max;
    return 0;
}

void VirtioNetAdapter::tick() {
}

} // namespace blockos::network
