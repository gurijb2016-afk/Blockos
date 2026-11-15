#pragma once
#include "../core/status.h"
#include <stddef.h>
#include <stdint.h>
namespace BlockOS { namespace Crypto {
constexpr size_t kSHA3_256DigestSize = 32;
CryptoResult SHA3_256(const uint8_t* input, size_t input_size, uint8_t* output, size_t output_size);
CryptoResult RegisterSHA3_256();
} }
