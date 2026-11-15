#pragma once
#include "../core/status.h"
#include "../hash/sha256.h"
#include <stddef.h>
#include <stdint.h>
namespace BlockOS { namespace Crypto {
struct HMACSHA256Context { SHA256Context inner; uint8_t outer_pad[64]; bool finalized; };
void HMACSHA256Init(HMACSHA256Context* context, const uint8_t* key, size_t key_length);
CryptoResult HMACSHA256Update(HMACSHA256Context* context, const uint8_t* data, size_t length);
CryptoResult HMACSHA256Final(HMACSHA256Context* context, uint8_t output[32]);
CryptoResult HMACSHA256(const uint8_t* key, size_t key_length, const uint8_t* data, size_t data_length, uint8_t output[32]);
} }
