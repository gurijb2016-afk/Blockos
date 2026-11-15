#include "self_test.h"
#include "../hash/sha256.h"
#include "secure_memory.h"
namespace BlockOS { namespace Crypto {
CryptoResult RunSelfTests() {
    static const uint8_t expected[32]={
      0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,
      0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad};
    uint8_t actual[32]={0};
    const uint8_t input[3]={'a','b','c'};
    CryptoResult result=SHA256(input,sizeof(input),actual,sizeof(actual));
    if (result!=CryptoResult::Success || !ConstantTimeEqual(actual,expected,sizeof(expected))) {
        SecureZero(actual,sizeof(actual)); return CryptoResult::SelfTestFailed;
    }
    SecureZero(actual,sizeof(actual));
    return CryptoResult::Success;
}
} }
