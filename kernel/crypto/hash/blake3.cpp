#include "../core/status.h"
namespace BlockOS { namespace Crypto { namespace Algorithms {
// BLAKE3: intentionally not implemented yet. Do not use for security decisions.
CryptoResult BLAKE3Unavailable() { return CryptoResult::NotImplemented; }
} } }
