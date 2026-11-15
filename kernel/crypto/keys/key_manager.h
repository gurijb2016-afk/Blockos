#pragma once
#include "../core/status.h"
#include <stddef.h>
#include <stdint.h>
namespace BlockOS { namespace Crypto {
constexpr size_t kMaxVolatileKeyBytes = 64;
enum KeyUsage : uint32_t {
    KeyUsageEncrypt = 1u << 0,
    KeyUsageDecrypt = 1u << 1,
    KeyUsageSign = 1u << 2,
    KeyUsageVerify = 1u << 3,
    KeyUsageDerive = 1u << 4,
    KeyUsageAuthenticate = 1u << 5
};
struct KeyHandle { uint64_t value; };
struct KeyMetadata { size_t material_size; uint32_t usage; bool active; };
using KeyUseCallback = CryptoResult (*)(const uint8_t* key, size_t key_length, void* context);
CryptoResult ImportVolatileKey(const uint8_t* material, size_t material_length, uint32_t usage, KeyHandle* handle);
CryptoResult GenerateVolatileKey(size_t key_length, uint32_t usage, KeyHandle* handle);
CryptoResult UseVolatileKey(KeyHandle handle, uint32_t required_usage, KeyUseCallback callback, void* context);
CryptoResult GetVolatileKeyMetadata(KeyHandle handle, KeyMetadata* metadata);
CryptoResult DestroyVolatileKey(KeyHandle handle);
void DestroyAllVolatileKeys();
} }
