#include "e1000.hpp"

namespace blockos::network {

bool E1000Adapter::bring_up() { up_ = true; return true; }
bool E1000Adapter::bring_down() { up_ = false; return true; }
bool E1000Adapter::send_frame(const void* data, std::size_t len) { (void)data; (void)len; return up_; }
std::size_t E1000Adapter::poll_frame(void* out, std::size_t max) { (void)out; (void)max; return 0; }
void E1000Adapter::tick() {}

} // namespace blockos::network
