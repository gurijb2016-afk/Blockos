#include "../core/status.h"
namespace BlockOS { namespace Crypto { namespace Algorithms {
// ECDSA: intentionally not implemented yet. Do not use for security decisions.
CryptoResult ECDSAUnavailable() { return CryptoResult::NotImplemented; }
} } }
