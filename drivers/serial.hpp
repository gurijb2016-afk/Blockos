#pragma once

#include <stdint.h>
#include <stddef.h>

namespace serial {

constexpr uint16_t COM1 = 0x3F8;
constexpr uint16_t COM2 = 0x2F8;
constexpr uint16_t COM3 = 0x3E8;
constexpr uint16_t COM4 = 0x2E8;

bool init(uint16_t port = COM1, uint32_t baud = 115200);
bool is_present(uint16_t port = COM1);
void write_char(char c, uint16_t port = COM1);
void write_string(const char* str, uint16_t port = COM1);
void write(const void* data, size_t len, uint16_t port = COM1);
bool has_data(uint16_t port = COM1);
char read_char(uint16_t port = COM1);
int16_t read_char_nonblocking(uint16_t port = COM1);

} // namespace serial
