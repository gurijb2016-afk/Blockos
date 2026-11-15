#include "csprng.h"
namespace BlockOS { namespace Crypto {
CryptoResult GenerateRandom(uint8_t* output, size_t length) { return CSPRNGFill(output, length); }
} }
