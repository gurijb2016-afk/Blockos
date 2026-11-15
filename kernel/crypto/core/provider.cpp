#include "provider.h"

namespace BlockOS { namespace Crypto {
namespace { const RandomProvider* g_random_provider = nullptr; }

CryptoResult RegisterRandomProvider(const RandomProvider* provider) {
    if (!provider || !provider->name || !provider->fill)
        return CryptoResult::InvalidArgument;
    if (!provider->approved_for_security_use)
        return CryptoResult::PermissionDenied;
    if (g_random_provider)
        return CryptoResult::AlreadyExists;
    g_random_provider = provider;
    return CryptoResult::Success;
}

CryptoResult GenerateRandom(uint8_t* output, size_t length) {
    if (length == 0) return CryptoResult::Success;
    if (!output) return CryptoResult::InvalidArgument;
    if (!g_random_provider) return CryptoResult::EntropyUnavailable;
    return g_random_provider->fill(output, length);
}

void ClearRandomProvider() { g_random_provider = nullptr; }

} }
