#include "lwip_adapter.hpp"
#include "virtio_net_driver.hpp"
#include <efi.h>
#include <efilib.h>

void lwip_adapter::init() {
    // In a full integration, here we would initialize lwIP, create a netif that uses
    // lwip_adapter::transmit() for sending frames and call netif->input() when data arrives.
    // This scaffold leaves lwIP out of tree; the adapter provides the integration points.
    if (!virtio_net::init()) {
        Print(L"lwip_adapter: virtio-net not available, lwIP cannot be attached\n");
        return;
    }
    Print(L"lwip_adapter: initialized (adapter only)\n");
}

void lwip_adapter::poll_receive() {
    uint8_t tmp[1600];
    int r = virtio_net::receive_packet(tmp, sizeof(tmp));
    if (r > 0) {
        // in a real integration we'd pass this packet to lwIP (e.g., netif->input)
        Print(L"lwip_adapter: received packet (len)\n");
    }
}

bool lwip_adapter::transmit(const void* data, unsigned len) {
    if (!virtio_net::is_available()) return false;
    return virtio_net::send_packet(data, len);
}
