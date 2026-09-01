#pragma once
#include <stdint.h>
#include <stddef.h>

namespace pcie {

struct Device {
    uint8_t bus;
    uint8_t slot;
    uint8_t func;
    uint16_t vendor;
    uint16_t device;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t header_type;
    uint8_t irq_line;
    uint8_t irq_pin;
};

struct Bar {
    uint64_t base;
    uint64_t size;
    bool is_io;
    bool is_64;
    bool prefetchable;
};

size_t enumerate(Device* out, size_t capacity);
bool read_bar(uint8_t bus, uint8_t slot, uint8_t func, unsigned index, Bar* out);
bool enable_memory_io(uint8_t bus, uint8_t slot, uint8_t func, bool bus_master);
bool find_class(uint8_t class_code, uint8_t subclass, Device* out);

}
