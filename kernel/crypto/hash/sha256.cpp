#include "sha256.h"
#include "../core/algorithm.h"
#include "../core/registry.h"
#include "../security/secure_memory.h"

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
inline uint32_t Ror(uint32_t x, unsigned n) { return (x >> n) | (x << (32u - n)); }
inline uint32_t LoadBE32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}
inline void StoreBE32(uint8_t* p, uint32_t x) {
    p[0]=uint8_t(x>>24); p[1]=uint8_t(x>>16); p[2]=uint8_t(x>>8); p[3]=uint8_t(x);
}
void Transform(uint32_t state[8], const uint8_t block[64]) {
    uint32_t w[64];
    for (unsigned i=0;i<16;++i) w[i]=LoadBE32(block+4*i);
    for (unsigned i=16;i<64;++i) {
        const uint32_t s0=Ror(w[i-15],7)^Ror(w[i-15],18)^(w[i-15]>>3);
        const uint32_t s1=Ror(w[i-2],17)^Ror(w[i-2],19)^(w[i-2]>>10);
        w[i]=w[i-16]+s0+w[i-7]+s1;
    }
    uint32_t a=state[0],b=state[1],c=state[2],d=state[3];
    uint32_t e=state[4],f=state[5],g=state[6],h=state[7];
    for (unsigned i=0;i<64;++i) {
        const uint32_t s1=Ror(e,6)^Ror(e,11)^Ror(e,25);
        const uint32_t ch=(e&f)^(~e&g);
        const uint32_t t1=h+s1+ch+K[i]+w[i];
        const uint32_t s0=Ror(a,2)^Ror(a,13)^Ror(a,22);
        const uint32_t maj=(a&b)^(a&c)^(b&c);
        const uint32_t t2=s0+maj;
        h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
    }
    state[0]+=a; state[1]+=b; state[2]+=c; state[3]+=d;
    state[4]+=e; state[5]+=f; state[6]+=g; state[7]+=h;
    SecureZero(w,sizeof(w));
}
}

void SHA256Init(SHA256Context* c) {
    if (!c) return;
    const uint32_t initial[8]={0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,
                               0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u};
    for (unsigned i=0;i<8;++i) c->state[i]=initial[i];
    c->total_bytes=0; c->block_used=0; c->finalized=false;
    for (unsigned i=0;i<64;++i) c->block[i]=0;
}

CryptoResult SHA256Update(SHA256Context* c, const uint8_t* input, size_t n) {
    if (!c || (!input && n)) return CryptoResult::InvalidArgument;
    if (c->finalized) return CryptoResult::PermissionDenied;
    if (n > UINT64_MAX - c->total_bytes) return CryptoResult::InvalidArgument;
    c->total_bytes += static_cast<uint64_t>(n);
    while (n) {
        size_t take=64-c->block_used;
        if (take>n) take=n;
        for (size_t i=0;i<take;++i) c->block[c->block_used+i]=input[i];
        c->block_used+=take; input+=take; n-=take;
        if (c->block_used==64) { Transform(c->state,c->block); c->block_used=0; }
    }
    return CryptoResult::Success;
}

CryptoResult SHA256Final(SHA256Context* c, uint8_t* output, size_t output_size) {
    if (!c || !output) return CryptoResult::InvalidArgument;
    if (output_size<kSHA256DigestSize) return CryptoResult::BufferTooSmall;
    if (c->finalized) return CryptoResult::PermissionDenied;
    const uint64_t bit_length=c->total_bytes*8u;
    c->block[c->block_used++]=0x80;
    if (c->block_used>56) {
        while (c->block_used<64) c->block[c->block_used++]=0;
        Transform(c->state,c->block); c->block_used=0;
    }
    while (c->block_used<56) c->block[c->block_used++]=0;
    for (unsigned i=0;i<8;++i) c->block[63-i]=uint8_t(bit_length>>(8u*i));
    Transform(c->state,c->block);
    for (unsigned i=0;i<8;++i) StoreBE32(output+4*i,c->state[i]);
    c->finalized=true;
    SecureZero(c->state,sizeof(c->state));
    SecureZero(c->block,sizeof(c->block));
    c->total_bytes=0; c->block_used=0;
    return CryptoResult::Success;
}

CryptoResult SHA256(const uint8_t* input, size_t input_size, uint8_t* output, size_t output_size) {
    if ((!input && input_size) || !output) return CryptoResult::InvalidArgument;
    if (output_size<kSHA256DigestSize) return CryptoResult::BufferTooSmall;
    SHA256Context c; SHA256Init(&c);
    CryptoResult r=SHA256Update(&c,input,input_size);
    if (r==CryptoResult::Success) r=SHA256Final(&c,output,output_size);
    SecureZero(&c,sizeof(c));
    return r;
}

CryptoResult RegisterSHA256() {
    static const AlgorithmDescriptor d={"SHA-256",1,AlgorithmType::Hash,kSHA256DigestSize,&SHA256};
    return RegisterAlgorithm(&d);
}

} }
