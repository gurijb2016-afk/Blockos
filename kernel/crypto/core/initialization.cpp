#include "crypto.h"
#include "registry.h"
#include "provider.h"
#include "../hash/sha256.h"
#include "../security/self_test.h"

namespace BlockOS { namespace Crypto {
namespace { bool g_initialized = false; }

CryptoResult Initialize() {
    if (g_initialized) return CryptoResult::Success;
    CryptoResult result = RegisterSHA256();
    if (result != CryptoResult::Success && result != CryptoResult::AlreadyExists)
        return result;
    result = RunSelfTests();
    if (result != CryptoResult::Success) {
        ResetAlgorithmRegistryForShutdown();
        return result;
    }
    g_initialized = true;
    return CryptoResult::Success;
}

void Shutdown() {
    ClearRandomProvider();
    ResetAlgorithmRegistryForShutdown();
    g_initialized = false;
}

bool IsInitialized() { return g_initialized; }

} }
