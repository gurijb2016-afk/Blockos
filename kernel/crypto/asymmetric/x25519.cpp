#include "../core/status.h"
namespace BlockOS { namespace Crypto { namespace Algorithms {
// X25519: intentionally not implemented yet. Do not use for security decisions.
CryptoResult X25519Unavailable() { return CryptoResult::NotImplemented; }
} } }
