#pragma once
#include "status.h"
#include "types.h"

namespace BlockOS { namespace Crypto {

CryptoResult Initialize();
void Shutdown();
bool IsInitialized();
CryptoResult Hash(const char* algorithm, const uint8_t* input, size_t input_size,
                  uint8_t* output, size_t output_size);
CryptoResult GenerateRandom(uint8_t* output, size_t length);
void SecureZero(void* memory, size_t length);
bool ConstantTimeEqual(const void* left, const void* right, size_t length);
uint32_t FrameworkApiVersion();
const char* ResultName(CryptoResult result);

} }
