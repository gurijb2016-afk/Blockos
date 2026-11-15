#include "../core/status.h"
namespace BlockOS { namespace Crypto { namespace Algorithms {
// PBKDF2: intentionally not implemented yet. Do not use for security decisions.
CryptoResult PBKDF2Unavailable() { return CryptoResult::NotImplemented; }
} } }
