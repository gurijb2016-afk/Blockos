#include "crypto.h"
#include "registry.h"

namespace BlockOS { namespace Crypto {
CryptoResult Hash(const char* algorithm, const uint8_t* input, size_t input_size,
                  uint8_t* output, size_t output_size) {
    if (!IsInitialized()) return CryptoResult::NotInitialized;
    if (!algorithm || (!input && input_size != 0) || !output)
        return CryptoResult::InvalidArgument;
    const AlgorithmDescriptor* descriptor = FindAlgorithm(algorithm, AlgorithmType::Hash);
    if (!descriptor) return CryptoResult::NotSupported;
    if (output_size < descriptor->output_size) return CryptoResult::BufferTooSmall;
    if (!descriptor->hash) return CryptoResult::NotImplemented;
    return descriptor->hash(input, input_size, output, output_size);
}
} }
