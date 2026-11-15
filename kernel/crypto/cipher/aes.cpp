#include "../core/status.h"
namespace BlockOS { namespace Crypto { namespace Algorithms {
// AES: intentionally not implemented yet. Do not use for security decisions.
CryptoResult AESUnavailable() { return CryptoResult::NotImplemented; }
} } }
