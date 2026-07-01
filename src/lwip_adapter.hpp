#pragma once

// Adapter stubs for integrating lwIP (or another IP stack) with the virtio-net driver.
// This file provides the hooks the stack expects: low-level transmit and receive callbacks.

namespace lwip_adapter {
    void init();
    // called by kernel main loop to pump incoming packets into the stack
    void poll_receive();
    // send raw ethernet frame via underlying driver
    bool transmit(const void* data, unsigned len);
}
