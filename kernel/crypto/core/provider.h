#pragma once
#include "status.h"
#include "types.h"

namespace BlockOS { namespace Crypto {

using RandomFillFunction = CryptoResult (*)(uint8_t* output, size_t length);

struct RandomProvider {
    const char* name;
    uint32_t version;
    RandomFillFunction fill;
    bool approved_for_security_use;
};

CryptoResult RegisterRandomProvider(const RandomProvider* provider);
CryptoResult GenerateRandom(uint8_t* output, size_t length);
void ClearRandomProvider();

} }
