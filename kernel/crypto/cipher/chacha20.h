#pragma once
#include "../core/status.h"
#include <stddef.h>
#include <stdint.h>
namespace BlockOS { namespace Crypto {
CryptoResult ChaCha20Xor(const uint8_t key[32], const uint8_t nonce[12], uint32_t initial_counter,
                         const uint8_t* input, uint8_t* output, size_t length);
} }
