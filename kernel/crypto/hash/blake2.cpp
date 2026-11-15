#include "../core/status.h"
namespace BlockOS { namespace Crypto { namespace Algorithms {
// BLAKE2: intentionally not implemented yet. Do not use for security decisions.
CryptoResult BLAKE2Unavailable() { return CryptoResult::NotImplemented; }
} } }
