#include "usb_hub_protocol.hpp"

namespace usb::hub {

bool append_route(const Route& parent, uint8_t downstream_port, Route* out)
{
    if (!out || downstream_port == 0 || downstream_port > 15 || parent.depth >= 5)
        return false;
    if (parent.string & ~((parent.depth == 0) ? 0u : ((1u << (parent.depth * 4u)) - 1u)))
        return false;
    out->string = parent.string | (static_cast<uint32_t>(downstream_port) << (parent.depth * 4u));
    out->depth = static_cast<uint8_t>(parent.depth + 1u);
    return true;
}

bool decode_port_status(const uint8_t* bytes, size_t length, PortStatus* out)
{
    if (!bytes || length < 4 || !out)
        return false;
    const uint16_t status = static_cast<uint16_t>(bytes[0] | (static_cast<uint16_t>(bytes[1]) << 8));
    const uint16_t change = static_cast<uint16_t>(bytes[2] | (static_cast<uint16_t>(bytes[3]) << 8));
    out->status = status;
    out->change = change;
    out->connected = (status & (1u << 0)) != 0;
    out->enabled = (status & (1u << 1)) != 0;
    out->resetting = (status & (1u << 4)) != 0;
    out->low_speed = (status & (1u << 9)) != 0;
    out->high_speed = (status & (1u << 10)) != 0;
    out->xhci_speed_id = out->high_speed ? 3u : (out->low_speed ? 2u : 1u);
    return true;
}

bool validate_hub_descriptor(const uint8_t* bytes, size_t length,
                             uint8_t* port_count, uint8_t* power_good_2ms)
{
    if (!bytes || length < 7 || !port_count || !power_good_2ms)
        return false;
    if (bytes[0] < 7 || bytes[0] > length || bytes[1] != 0x29 || bytes[2] == 0)
        return false;
    *port_count = bytes[2];
    *power_good_2ms = bytes[5];
    return true;
}

} // namespace usb::hub
