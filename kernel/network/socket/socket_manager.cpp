#include "socket_manager.hpp"
#include <cstring>

namespace blockos::network {

SocketManager::SocketManager(NetworkManager& net) : net_(net) {}

SocketManager::Socket* SocketManager::get(int fd) {
    if (fd < 0 || fd >= static_cast<int>(NET_MAX_SOCKETS)) return nullptr;
    return sockets_[fd].used ? &sockets_[fd] : nullptr;
}

const SocketManager::Socket* SocketManager::get(int fd) const {
    if (fd < 0 || fd >= static_cast<int>(NET_MAX_SOCKETS)) return nullptr;
    return sockets_[fd].used ? &sockets_[fd] : nullptr;
}

int SocketManager::create() {
    for (int i = 0; i < static_cast<int>(NET_MAX_SOCKETS); ++i) {
        if (!sockets_[i].used) {
            sockets_[i] = {};
            sockets_[i].used = true;
            sockets_[i].fd = static_cast<std::uint32_t>(i);
            sockets_[i].state = SocketState::Closed;
            return i;
        }
    }
    return -1;
}

bool SocketManager::bind(int fd, const IPv4Address& ip, std::uint16_t port) {
    Socket* s = get(fd);
    if (!s) return false;
    s->local_ip = ip;
    s->local_port = port;
    s->state = SocketState::Bound;
    return true;
}

bool SocketManager::listen(int fd, int backlog) {
    (void)backlog;
    Socket* s = get(fd);
    if (!s) return false;
    s->state = SocketState::Listening;
    return true;
}

bool SocketManager::connect(int fd, const IPv4Address& ip, std::uint16_t port) {
    Socket* s = get(fd);
    if (!s) return false;
    s->remote_ip = ip;
    s->remote_port = port;
    s->state = SocketState::Established;
    return true;
}

std::size_t SocketManager::send(int fd, const void* data, std::size_t len) {
    Socket* s = get(fd);
    if (!s || !data || len == 0) return 0;
    if (s->state != SocketState::Established && s->state != SocketState::Listening) return 0;
    return net_.send_raw(data, len) ? len : 0;
}

std::size_t SocketManager::recv(int fd, void* out, std::size_t max) {
    Socket* s = get(fd);
    if (!s || !out || max == 0) return 0;
    return net_.recv_raw(out, max);
}

bool SocketManager::shutdown(int fd) {
    Socket* s = get(fd);
    if (!s) return false;
    s->state = SocketState::Shutdown;
    return true;
}

bool SocketManager::close(int fd) {
    Socket* s = get(fd);
    if (!s) return false;
    s->used = false;
    s->state = SocketState::Closed;
    return true;
}

SocketState SocketManager::state(int fd) const {
    const Socket* s = get(fd);
    return s ? s->state : SocketState::Closed;
}

} // namespace blockos::network
