#pragma once
#include "../core/status.h"
#include <stddef.h>
#include <stdint.h>
namespace BlockOS { namespace Crypto {
// Platform callback must return cryptographically sound entropy and report failure.
using EntropySource = CryptoResult (*)(uint8_t* output, size_t length);
CryptoResult AddEntropySource(EntropySource source);
CryptoResult CollectEntropy(uint8_t* output, size_t length);
} }
