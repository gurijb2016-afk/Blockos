#include "sha3.h"
#include "../core/algorithm.h"
#include "../core/registry.h"
#include "../security/secure_memory.h"
namespace BlockOS { namespace Crypto {
namespace {
static const uint64_t RC[24]={
0x0000000000000001ULL,0x0000000000008082ULL,0x800000000000808aULL,0x8000000080008000ULL,
0x000000000000808bULL,0x0000000080000001ULL,0x8000000080008081ULL,0x8000000000008009ULL,
0x000000000000008aULL,0x0000000000000088ULL,0x0000000080008009ULL,0x000000008000000aULL,
0x000000008000808bULL,0x800000000000008bULL,0x8000000000008089ULL,0x8000000000008003ULL,
0x8000000000008002ULL,0x8000000000000080ULL,0x000000000000800aULL,0x800000008000000aULL,
0x8000000080008081ULL,0x8000000000008080ULL,0x0000000080000001ULL,0x8000000080008008ULL};
static const unsigned ROT[25]={0,1,62,28,27,36,44,6,55,20,3,10,43,25,39,41,45,15,21,8,18,2,61,56,14};
inline uint64_t Rol(uint64_t x,unsigned n){return n?((x<<n)|(x>>(64u-n))):x;}
void Permute(uint64_t a[25]){
 for(unsigned round=0;round<24;++round){
  uint64_t c[5],d[5],b[25];
  for(unsigned x=0;x<5;++x)c[x]=a[x]^a[x+5]^a[x+10]^a[x+15]^a[x+20];
  for(unsigned x=0;x<5;++x)d[x]=c[(x+4)%5]^Rol(c[(x+1)%5],1);
  for(unsigned y=0;y<5;++y)for(unsigned x=0;x<5;++x)a[x+5*y]^=d[x];
  for(unsigned y=0;y<5;++y)for(unsigned x=0;x<5;++x){unsigned nx=y,ny=(2*x+3*y)%5;b[nx+5*ny]=Rol(a[x+5*y],ROT[x+5*y]);}
  for(unsigned y=0;y<5;++y)for(unsigned x=0;x<5;++x)a[x+5*y]=b[x+5*y]^((~b[(x+1)%5+5*y])&b[(x+2)%5+5*y]);
  a[0]^=RC[round];SecureZero(c,sizeof(c));SecureZero(d,sizeof(d));SecureZero(b,sizeof(b));
 }
}
void XorBlock(uint64_t a[25],const uint8_t* p){for(unsigned i=0;i<17;++i){uint64_t v=0;for(unsigned j=0;j<8;++j)v|=uint64_t(p[i*8+j])<<(8u*j);a[i]^=v;}}
}
CryptoResult SHA3_256(const uint8_t*in,size_t n,uint8_t*out,size_t outn){
 if((!in&&n)||!out) return CryptoResult::InvalidArgument;
 if(outn<32) return CryptoResult::BufferTooSmall;
 uint64_t a[25]={0};size_t off=0;while(n-off>=136){XorBlock(a,in+off);Permute(a);off+=136;}
 uint8_t last[136]={0};size_t rem=n-off;for(size_t i=0;i<rem;++i)last[i]=in[off+i];last[rem]=0x06;last[135]|=0x80;XorBlock(a,last);Permute(a);
 for(unsigned i=0;i<32;++i)out[i]=uint8_t(a[i/8]>>(8u*(i%8)));
 SecureZero(a,sizeof(a));SecureZero(last,sizeof(last));return CryptoResult::Success;
}
CryptoResult RegisterSHA3_256(){static const AlgorithmDescriptor d={"SHA3-256",1,AlgorithmType::Hash,kSHA3_256DigestSize,&SHA3_256};return RegisterAlgorithm(&d);}
} }
