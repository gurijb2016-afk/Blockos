#pragma once
#include "algorithm.h"

namespace BlockOS { namespace Crypto {

constexpr size_t kMaxAlgorithms = 64;

CryptoResult RegisterAlgorithm(const AlgorithmDescriptor* descriptor);
CryptoResult UnregisterAlgorithm(const char* name);
const AlgorithmDescriptor* FindAlgorithm(const char* name, AlgorithmType type);
size_t RegisteredAlgorithmCount();
void ResetAlgorithmRegistryForShutdown();

} }
