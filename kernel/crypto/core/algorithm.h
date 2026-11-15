#pragma once
#include "status.h"
#include "types.h"

namespace BlockOS { namespace Crypto {

using HashFunction = CryptoResult (*)(const uint8_t*, size_t, uint8_t*, size_t);

struct AlgorithmDescriptor {
    const char* name;
    uint32_t version;
    AlgorithmType type;
    size_t output_size;
    HashFunction hash;
};

} }
