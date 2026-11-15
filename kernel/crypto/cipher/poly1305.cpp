#include "../core/status.h"
namespace BlockOS { namespace Crypto { namespace Algorithms {
// Poly1305: intentionally not implemented yet. Do not use for security decisions.
CryptoResult Poly1305Unavailable() { return CryptoResult::NotImplemented; }
} } }
