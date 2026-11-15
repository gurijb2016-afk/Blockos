#pragma once
#include "../core/status.h"
#include <stddef.h>
#include <stdint.h>
namespace BlockOS { namespace Crypto {
CryptoResult CSPRNGInitialize();
CryptoResult CSPRNGFill(uint8_t* output, size_t length);
CryptoResult CSPRNGReseed(const uint8_t* entropy, size_t length);
} }
