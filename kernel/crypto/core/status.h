#pragma once
#include <stdint.h>

namespace BlockOS { namespace Crypto {

enum class CryptoResult : uint32_t {
    Success = 0,
    InvalidArgument,
    BufferTooSmall,
    NotFound,
    AlreadyExists,
    RegistryFull,
    NotInitialized,
    NotSupported,
    NotImplemented,
    SelfTestFailed,
    EntropyUnavailable,
    VerificationFailed,
    PermissionDenied,
    InternalError
};

} }
