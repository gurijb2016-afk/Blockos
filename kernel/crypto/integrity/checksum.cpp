#include "checksum.h"
#include "../hash/sha256.h"
namespace BlockOS { namespace Crypto {
CryptoResult ComputeSHA256Checksum(const uint8_t*data,size_t n,uint8_t out[32]){return SHA256(data,n,out,32);}
} }
