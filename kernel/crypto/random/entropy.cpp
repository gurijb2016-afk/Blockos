#include "entropy.h"
namespace BlockOS { namespace Crypto {
namespace { constexpr size_t kMaxSources=8; EntropySource g_sources[kMaxSources]={}; size_t g_count=0; }
CryptoResult AddEntropySource(EntropySource source) {
    if (!source) return CryptoResult::InvalidArgument;
    if (g_count>=kMaxSources) return CryptoResult::RegistryFull;
    g_sources[g_count++]=source; return CryptoResult::Success;
}
CryptoResult CollectEntropy(uint8_t* output, size_t length) {
    if ((!output && length) || length==0) return length==0?CryptoResult::Success:CryptoResult::InvalidArgument;
    if (g_count==0) return CryptoResult::EntropyUnavailable;
    // This interface deliberately does not invent an entropy mixer. The platform
    // CSPRNG provider must own conditioning, health tests and reseed policy.
    return CryptoResult::NotImplemented;
}
} }
