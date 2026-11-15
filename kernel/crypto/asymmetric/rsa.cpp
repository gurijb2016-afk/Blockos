#include "../core/status.h"
namespace BlockOS { namespace Crypto { namespace Algorithms {
// RSA: intentionally not implemented yet. Do not use for security decisions.
CryptoResult RSAUnavailable() { return CryptoResult::NotImplemented; }
} } }
