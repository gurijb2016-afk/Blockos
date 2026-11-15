#include "csprng.h"
#include "../core/crypto.h"
namespace BlockOS { namespace Crypto {
CryptoResult CSPRNGInitialize() { return CryptoResult::EntropyUnavailable; }
CryptoResult CSPRNGFill(uint8_t* output, size_t length) { return GenerateRandom(output,length); }
CryptoResult CSPRNGReseed(const uint8_t* entropy, size_t length) {
    if (!entropy || length==0) return CryptoResult::InvalidArgument;
    return CryptoResult::NotImplemented;
}
} }
