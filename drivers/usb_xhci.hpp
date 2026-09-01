#pragma once
#include <stdint.h>
#include <stddef.h>

namespace usb::xhci {

struct Controller {
    uint8_t bus, slot, func;
    volatile uint8_t* mmio;
    uint32_t cap_length;
    uint32_t version;
    uint32_t dboff;
    uint32_t rtsoff;
    uint8_t max_ports;
    uint8_t context_bytes;
};

bool probe(Controller* out);
bool stop(Controller* c);
bool reset(Controller* c);
bool run(Controller* c);
uint32_t read_port_status(const Controller* c, unsigned port);
bool reset_port(const Controller* c, unsigned port);

}
