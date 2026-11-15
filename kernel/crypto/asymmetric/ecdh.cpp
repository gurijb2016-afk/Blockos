#include "../core/status.h"
namespace BlockOS { namespace Crypto { namespace Algorithms {
// ECDH: intentionally not implemented yet. Do not use for security decisions.
CryptoResult ECDHUnavailable() { return CryptoResult::NotImplemented; }
} } }
