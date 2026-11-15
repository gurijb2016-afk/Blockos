#include "../core/status.h"
namespace BlockOS { namespace Crypto { namespace Algorithms {
// Ed25519: intentionally not implemented yet. Do not use for security decisions.
CryptoResult Ed25519Unavailable() { return CryptoResult::NotImplemented; }
} } }
