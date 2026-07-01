#pragma once
#include <cstdint>

// Virtio-net driver API (raw ethernet frames)
namespace virtio_net {
    bool init(); // initialize virtio-net device (probe + virtqueue setup)
    bool is_available();
    // send raw ethernet frame (dst buffer must remain valid until send returns)
    bool send_packet(const void* data, unsigned len);
    // receive packet into buffer, returns length or -1 if none
    int receive_packet(void* buf, unsigned buf_len);
}
