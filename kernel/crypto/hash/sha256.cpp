#include "sha256.h"
#include "../core/algorithm.h"
#include "../core/registry.h"

namespace BlockOS { namespace Crypto {
namespace {
static const uint32_t K[64] = {
  0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
  0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
  0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
  0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
  0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
  0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
  0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
  0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
};
inline uint32_t R(uint32_t x, unsigned n) { return (x >> n) | (x << (32u - n)); }
inline uint32_t LoadBE(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}
inline void StoreBE(uint8_t* p, uint32_t x) {
    p[0]=uint8_t(x>>24); p[1]=uint8_t(x>>16); p[2]=uint8_t(x>>8); p[3]=uint8_t(x);
}
void Transform(uint32_t state[8], const uint8_t block[64]) {
    uint32_t w[64];
    for (unsigned i=0;i<16;++i) w[i]=LoadBE(block+4*i);
    for (unsigned i=16;i<64;++i) {
        uint32_t s0=R(w[i-15],7)^R(w[i-15],18)^(w[i-15]>>3);
        uint32_t s1=R(w[i-2],17)^R(w[i-2],19)^(w[i-2]>>10);
        w[i]=w[i-16]+s0+w[i-7]+s1;
    }
    uint32_t a=state[0],b=state[1],c=state[2],d=state[3];
    uint32_t e=state[4],f=state[5],g=state[6],h=state[7];
    for (unsigned i=0;i<64;++i) {
        uint32_t s1=R(e,6)^R(e,11)^R(e,25);
        uint32_t ch=(e&f)^(~e&g);
        uint32_t t1=h+s1+ch+K[i]+w[i];
        uint32_t s0=R(a,2)^R(a,13)^R(a,22);
        uint32_t maj=(a&b)^(a&c)^(b&c);
        uint32_t t2=s0+maj;
        h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
    }
    state[0]+=a; state[1]+=b; state[2]+=c; state[3]+=d;
    state[4]+=e; state[5]+=f; state[6]+=g; state[7]+=h;
}
}

CryptoResult SHA256(const uint8_t* input, size_t input_size,
                    uint8_t* output, size_t output_size) {
    if ((!input && input_size != 0) || !output) return CryptoResult::InvalidArgument;
    if (output_size < kSHA256DigestSize) return CryptoResult::BufferTooSmall;
    uint32_t state[8]={0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,
                       0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u};
    size_t offset=0;
    while (input_size-offset >= 64) { Transform(state,input+offset); offset+=64; }
    uint8_t tail[128]={0};
    const size_t remaining=input_size-offset;
    for (size_t i=0;i<remaining;++i) tail[i]=input[offset+i];
    tail[remaining]=0x80;
    const size_t padded=(remaining<56)?64:128;
    const uint64_t bit_length=static_cast<uint64_t>(input_size)*8u;
    for (unsigned i=0;i<8;++i) tail[padded-1-i]=uint8_t(bit_length>>(8u*i));
    Transform(state,tail);
    if (padded==128) Transform(state,tail+64);
    for (unsigned i=0;i<8;++i) StoreBE(output+4*i,state[i]);
    // Clear working state and temporary message block.
    volatile uint32_t* s=state; for (unsigned i=0;i<8;++i) s[i]=0;
    volatile uint8_t* t=tail; for (unsigned i=0;i<128;++i) t[i]=0;
    return CryptoResult::Success;
}

CryptoResult RegisterSHA256() {
    static const AlgorithmDescriptor descriptor={"SHA-256",1,AlgorithmType::Hash,kSHA256DigestSize,&SHA256};
    return RegisterAlgorithm(&descriptor);
}

} }
