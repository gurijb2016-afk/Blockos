#pragma once
#include "../core/status.h"
#include <stddef.h>
#include <stdint.h>
namespace BlockOS { namespace Crypto {
CryptoResult HKDFSHA256(const uint8_t* salt, size_t salt_length,
    const uint8_t* input_key, size_t input_key_length,
    const uint8_t* info, size_t info_length,
    uint8_t* output_key, size_t output_key_length);
} }
