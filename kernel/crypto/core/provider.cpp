#include "provider.h"
namespace BlockOS { namespace Crypto {
namespace { const RandomProvider* g_random_provider = nullptr; }
CryptoResult RegisterRandomProvider(const RandomProvider* provider) {
    if (!provider || !provider->name || provider->name[0] == '\0' || provider->version == 0 || !provider->fill) return CryptoResult::InvalidArgument;
    if (!provider->approved_for_security_use) return CryptoResult::PermissionDenied;
    if (g_random_provider) return CryptoResult::AlreadyExists;
    g_random_provider = provider;
    return CryptoResult::Success;
}
CryptoResult CollectPlatformEntropy(uint8_t* output, size_t length) {
    if ((!output && length) || length == 0) return length == 0 ? CryptoResult::Success : CryptoResult::InvalidArgument;
    if (!g_random_provider) return CryptoResult::EntropyUnavailable;
    return g_random_provider->fill(output, length);
}
void ClearRandomProvider() { g_random_provider = nullptr; }
} }
