#include "../core/status.h"
namespace BlockOS { namespace Crypto { namespace Algorithms {
// XChaCha20: intentionally not implemented yet. Do not use for security decisions.
CryptoResult XChaCha20Unavailable() { return CryptoResult::NotImplemented; }
} } }
