#pragma once
#include "../core/status.h"
#include <stddef.h>
#include <stdint.h>
namespace BlockOS { namespace Crypto {
constexpr size_t kBLAKE2sDigestSize = 32;
CryptoResult BLAKE2s256(const uint8_t* input, size_t input_size, uint8_t* output, size_t output_size);
CryptoResult RegisterBLAKE2s256();
} }
