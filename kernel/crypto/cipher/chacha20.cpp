#include "../core/status.h"
namespace BlockOS { namespace Crypto { namespace Algorithms {
// ChaCha20: intentionally not implemented yet. Do not use for security decisions.
CryptoResult ChaCha20Unavailable() { return CryptoResult::NotImplemented; }
} } }
