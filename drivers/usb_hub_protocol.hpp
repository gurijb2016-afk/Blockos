#pragma once

#include <stddef.h>
#include <stdint.h>

namespace usb::hub {

struct Route {
    uint32_t string;
    uint8_t depth;
};

struct PortStatus {
    uint16_t status;
    uint16_t change;
    bool connected;
    bool enabled;
    bool resetting;
    bool low_speed;
    bool high_speed;
    uint8_t xhci_speed_id;
};

// Route strings encode one 4-bit downstream port number per hub level.
// The root-hub port is carried separately in the xHCI slot context.
bool append_route(const Route& parent, uint8_t downstream_port, Route* out);

// Decode the four-byte USB 2 hub GET_STATUS response.
bool decode_port_status(const uint8_t* bytes, size_t length, PortStatus* out);

// Validate a USB 2 hub descriptor header (first seven bytes are required).
bool validate_hub_descriptor(const uint8_t* bytes, size_t length,
                             uint8_t* port_count, uint8_t* power_good_2ms);

} // namespace usb::hub
