#include "../core/status.h"
namespace BlockOS { namespace Crypto { namespace Algorithms {
// SHA512: intentionally not implemented yet. Do not use for security decisions.
CryptoResult SHA512Unavailable() { return CryptoResult::NotImplemented; }
} } }
