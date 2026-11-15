#pragma once
#include "../core/status.h"
#include <stddef.h>
#include <stdint.h>
namespace BlockOS { namespace Crypto {
CryptoResult PBKDF2_HMAC_SHA256(const uint8_t* password, size_t password_length,
    const uint8_t* salt, size_t salt_length, uint32_t iterations,
    uint8_t* output, size_t output_length);
} }
