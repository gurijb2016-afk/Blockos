#include "rtl8139.hpp"

namespace blockos::network {

bool Rtl8139Adapter::bring_up() { up_ = true; return true; }
bool Rtl8139Adapter::bring_down() { up_ = false; return true; }
bool Rtl8139Adapter::send_frame(const void* data, std::size_t len) { (void)data; (void)len; return up_; }
std::size_t Rtl8139Adapter::poll_frame(void* out, std::size_t max) { (void)out; (void)max; return 0; }
void Rtl8139Adapter::tick() {}

} // namespace blockos::network
