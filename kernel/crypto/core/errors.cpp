#include "status.h"
namespace BlockOS { namespace Crypto {
const char* ResultName(CryptoResult result) {
    switch (result) {
    case CryptoResult::Success: return "success";
    case CryptoResult::InvalidArgument: return "invalid argument";
    case CryptoResult::BufferTooSmall: return "buffer too small";
    case CryptoResult::NotFound: return "not found";
    case CryptoResult::AlreadyExists: return "already exists";
    case CryptoResult::RegistryFull: return "registry full";
    case CryptoResult::NotInitialized: return "not initialized";
    case CryptoResult::NotSupported: return "not supported";
    case CryptoResult::NotImplemented: return "not implemented";
    case CryptoResult::SelfTestFailed: return "self-test failed";
    case CryptoResult::EntropyUnavailable: return "entropy unavailable";
    case CryptoResult::VerificationFailed: return "verification failed";
    case CryptoResult::PermissionDenied: return "permission denied";
    default: return "internal error";
    }
}
} }
