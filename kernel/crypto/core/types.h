#pragma once
#include <stddef.h>
#include <stdint.h>

namespace BlockOS { namespace Crypto {

enum class AlgorithmType : uint32_t {
    Hash = 1,
    Aead = 2,
    Signature = 3,
    KeyExchange = 4,
    Kdf = 5,
    Random = 6
};

struct ByteSpan {
    const uint8_t* data;
    size_t size;
};

struct MutableByteSpan {
    uint8_t* data;
    size_t size;
};

} }
