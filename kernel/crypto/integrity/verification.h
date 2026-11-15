#pragma once
#include "../core/status.h"
#include <stddef.h>
#include <stdint.h>
namespace BlockOS { namespace Crypto {
CryptoResult VerifySHA256Checksum(const uint8_t* data, size_t length, const uint8_t expected[32]);
} }
