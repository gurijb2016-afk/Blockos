#include "../core/status.h"
namespace BlockOS { namespace Crypto { namespace Algorithms {
// AES256_GCM: intentionally not implemented yet. Do not use for security decisions.
CryptoResult AES256_GCMUnavailable() { return CryptoResult::NotImplemented; }
} } }
