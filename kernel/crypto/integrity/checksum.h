#pragma once
#include "../core/status.h"
#include <stddef.h>
#include <stdint.h>
namespace BlockOS { namespace Crypto {
CryptoResult ComputeSHA256Checksum(const uint8_t* data, size_t length, uint8_t checksum[32]);
} }
