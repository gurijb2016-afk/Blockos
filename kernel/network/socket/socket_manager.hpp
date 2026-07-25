#pragma once

#include "network_types.hpp"
#include "network_manager.hpp"

namespace blockos::network {

class SocketManager {
public:
    struct Socket {
        bool used = false;
        std::uint32_t fd = 0;
        SocketState state = SocketState::Closed;
        IPv4Address local_ip{};
        IPv4Address remote_ip{};
        std::uint16_t local_port = 0;
        std::uint16_t remote_port = 0;
        std::uint8_t queue[NET_MAX_QUEUE][NET_MAX_PAYLOAD]{};
        std::size_t queue_len[NET_MAX_QUEUE]{};
        std::size_t queue_count = 0;
    };

    explicit SocketManager(NetworkManager& net);

    int create();
    bool bind(int fd, const IPv4Address& ip, std::uint16_t port);
    bool listen(int fd, int backlog);
    bool connect(int fd, const IPv4Address& ip, std::uint16_t port);
    std::size_t send(int fd, const void* data, std::size_t len);
    std::size_t recv(int fd, void* out, std::size_t max);
    bool shutdown(int fd);
    bool close(int fd);

    SocketState state(int fd) const;

private:
    NetworkManager& net_;
    Socket sockets_[NET_MAX_SOCKETS]{};

    Socket* get(int fd);
    const Socket* get(int fd) const;
};

} // namespace blockos::network
