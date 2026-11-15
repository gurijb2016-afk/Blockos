#include "verification.h"
#include "../hash/sha256.h"
#include "../security/secure_memory.h"
namespace BlockOS { namespace Crypto {
CryptoResult VerifySHA256Checksum(const uint8_t*data,size_t n,const uint8_t expected[32]){
 if((!data&&n)||!expected) return CryptoResult::InvalidArgument;
 uint8_t actual[32]; CryptoResult r=SHA256(data,n,actual,sizeof(actual));
 if(r==CryptoResult::Success&&!ConstantTimeEqual(actual,expected,sizeof(actual))) r=CryptoResult::VerificationFailed;
 SecureZero(actual,sizeof(actual)); return r;
}
} }
