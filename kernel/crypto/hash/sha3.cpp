#include "../core/status.h"
namespace BlockOS { namespace Crypto { namespace Algorithms {
// SHA3: intentionally not implemented yet. Do not use for security decisions.
CryptoResult SHA3Unavailable() { return CryptoResult::NotImplemented; }
} } }
