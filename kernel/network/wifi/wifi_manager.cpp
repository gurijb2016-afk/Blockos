#include "wifi_manager.hpp"

namespace blockos::network {

bool WifiAdapter::bring_up() { up_ = true; return true; }
bool WifiAdapter::bring_down() { up_ = false; return true; }
bool WifiAdapter::send_frame(const void* data, std::size_t len) { (void)data; (void)len; return up_; }
std::size_t WifiAdapter::poll_frame(void* out, std::size_t max) { (void)out; (void)max; return 0; }
void WifiAdapter::tick() {}

} // namespace blockos::network
