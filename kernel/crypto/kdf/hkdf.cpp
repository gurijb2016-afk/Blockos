#include "../core/status.h"
namespace BlockOS { namespace Crypto { namespace Algorithms {
// HKDF: intentionally not implemented yet. Do not use for security decisions.
CryptoResult HKDFUnavailable() { return CryptoResult::NotImplemented; }
} } }
