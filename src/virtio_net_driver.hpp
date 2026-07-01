#pragma once

namespace virtio_net {
    bool init();
    bool is_available();
    bool send_packet(const void* data, unsigned len);
    int receive_packet(void* buf, unsigned buf_len);
    // Reclaim completed TX descriptors and recycle buffers
    void reclaim_tx();
}
