#include "crypto.h"
#include "registry.h"
#include "provider.h"
#include "../hash/sha256.h"
#include "../hash/sha512.h"
#include "../hash/sha3.h"
#include "../hash/blake2s.h"
#include "../random/csprng.h"
#include "../keys/key_manager.h"
#include "../security/self_test.h"

namespace BlockOS { namespace Crypto {
namespace { bool g_initialized = false; }

CryptoResult Initialize() {
    if (g_initialized) return CryptoResult::Success;
    CryptoResult result = RegisterSHA256();
    if (result != CryptoResult::Success && result != CryptoResult::AlreadyExists) { ResetAlgorithmRegistryForShutdown(); return result; }
    result = RegisterSHA512();
    if (result != CryptoResult::Success && result != CryptoResult::AlreadyExists) { ResetAlgorithmRegistryForShutdown(); return result; }
    result = RegisterSHA3_256();
    if (result != CryptoResult::Success && result != CryptoResult::AlreadyExists) { ResetAlgorithmRegistryForShutdown(); return result; }
    result = RegisterBLAKE2s256();
    if (result != CryptoResult::Success && result != CryptoResult::AlreadyExists) { ResetAlgorithmRegistryForShutdown(); return result; }
    result = RunSelfTests();
    if (result != CryptoResult::Success) {
        ResetAlgorithmRegistryForShutdown();
        return result;
    }
    g_initialized = true;
    return CryptoResult::Success;
}

void Shutdown() {
    CSPRNGShutdown();
    DestroyAllVolatileKeys();
    ClearRandomProvider();
    ResetAlgorithmRegistryForShutdown();
    g_initialized = false;
}

bool IsInitialized() { return g_initialized; }

} }
