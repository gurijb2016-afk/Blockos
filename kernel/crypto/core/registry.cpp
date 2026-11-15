#include "registry.h"

namespace BlockOS { namespace Crypto {
namespace {
const AlgorithmDescriptor* g_algorithms[kMaxAlgorithms] = {};
size_t g_count = 0;

bool SameName(const char* a, const char* b) {
    if (!a || !b) return false;
    while (*a && *b) { if (*a != *b) return false; ++a; ++b; }
    return *a == *b;
}
}

CryptoResult RegisterAlgorithm(const AlgorithmDescriptor* descriptor) {
    if (!descriptor || !descriptor->name || descriptor->name[0] == '\0')
        return CryptoResult::InvalidArgument;
    for (size_t i = 0; i < g_count; ++i) {
        if (SameName(g_algorithms[i]->name, descriptor->name))
            return CryptoResult::AlreadyExists;
    }
    if (g_count >= kMaxAlgorithms) return CryptoResult::RegistryFull;
    g_algorithms[g_count++] = descriptor;
    return CryptoResult::Success;
}

CryptoResult UnregisterAlgorithm(const char* name) {
    if (!name) return CryptoResult::InvalidArgument;
    for (size_t i = 0; i < g_count; ++i) {
        if (SameName(g_algorithms[i]->name, name)) {
            for (size_t j = i + 1; j < g_count; ++j)
                g_algorithms[j - 1] = g_algorithms[j];
            g_algorithms[--g_count] = nullptr;
            return CryptoResult::Success;
        }
    }
    return CryptoResult::NotFound;
}

const AlgorithmDescriptor* FindAlgorithm(const char* name, AlgorithmType type) {
    if (!name) return nullptr;
    for (size_t i = 0; i < g_count; ++i) {
        if (g_algorithms[i]->type == type && SameName(g_algorithms[i]->name, name))
            return g_algorithms[i];
    }
    return nullptr;
}

size_t RegisteredAlgorithmCount() { return g_count; }

void ResetAlgorithmRegistryForShutdown() {
    for (size_t i = 0; i < kMaxAlgorithms; ++i) g_algorithms[i] = nullptr;
    g_count = 0;
}

} }
