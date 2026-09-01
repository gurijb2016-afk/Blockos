#pragma once
#include <stdint.h>
#include <stddef.h>
namespace usb::hid {
struct KeyboardReport { uint8_t modifiers; uint8_t reserved; uint8_t keys[6]; };
struct MouseReport { uint8_t buttons; int8_t x; int8_t y; int8_t wheel; };
bool parse_boot_keyboard(const uint8_t* data,size_t len,KeyboardReport* out);
bool parse_boot_mouse(const uint8_t* data,size_t len,MouseReport* out);
const char* key_name(uint8_t usage);
}
