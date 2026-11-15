#pragma once
#include "../core/status.h"
#include <stddef.h>
#include <stdint.h>

namespace BlockOS { namespace Crypto {

constexpr size_t kSHA256DigestSize = 32;

struct SHA256Context {
    uint32_t state[8];
    uint64_t total_bytes;
    uint8_t block[64];
    size_t block_used;
    bool finalized;
};

void SHA256Init(SHA256Context* context);
CryptoResult SHA256Update(SHA256Context* context, const uint8_t* input, size_t input_size);
CryptoResult SHA256Final(SHA256Context* context, uint8_t* output, size_t output_size);
CryptoResult SHA256(const uint8_t* input, size_t input_size,
                    uint8_t* output, size_t output_size);
CryptoResult RegisterSHA256();

} }
