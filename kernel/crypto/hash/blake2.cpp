#include "blake2s.h"
#include "../core/algorithm.h"
#include "../core/registry.h"
#include "../security/secure_memory.h"
namespace BlockOS { namespace Crypto {
namespace {
static const uint32_t IV[8]={0x6A09E667u,0xBB67AE85u,0x3C6EF372u,0xA54FF53Au,0x510E527Fu,0x9B05688Cu,0x1F83D9ABu,0x5BE0CD19u};
static const uint8_t SIGMA[10][16]={
{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},{14,10,4,8,9,15,13,6,1,12,0,2,11,7,5,3},
{11,8,12,0,5,2,15,13,10,14,3,6,7,1,9,4},{7,9,3,1,13,12,11,14,2,6,5,10,4,0,15,8},
{9,0,5,7,2,4,10,15,14,1,11,12,6,8,3,13},{2,12,6,10,0,11,8,3,4,13,7,5,15,14,1,9},
{12,5,1,15,14,13,4,10,0,7,6,3,9,2,8,11},{13,11,7,14,12,1,3,9,5,0,15,4,8,6,2,10},
{6,15,14,9,11,3,0,8,12,2,13,7,1,4,10,5},{10,2,8,4,7,6,1,5,15,11,9,14,3,12,13,0}};
inline uint32_t Load32(const uint8_t*p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
inline void Store32(uint8_t*p,uint32_t x){p[0]=uint8_t(x);p[1]=uint8_t(x>>8);p[2]=uint8_t(x>>16);p[3]=uint8_t(x>>24);}
inline uint32_t Ror(uint32_t x,unsigned n){return(x>>n)|(x<<(32u-n));}
void G(uint32_t v[16],unsigned a,unsigned b,unsigned c,unsigned d,uint32_t x,uint32_t y){
 v[a]=v[a]+v[b]+x;v[d]=Ror(v[d]^v[a],16);v[c]+=v[d];v[b]=Ror(v[b]^v[c],12);
 v[a]=v[a]+v[b]+y;v[d]=Ror(v[d]^v[a],8);v[c]+=v[d];v[b]=Ror(v[b]^v[c],7);
}
void Compress(uint32_t h[8],const uint8_t block[64],uint64_t t,bool last){
 uint32_t m[16],v[16];for(unsigned i=0;i<16;++i)m[i]=Load32(block+4*i);
 for(unsigned i=0;i<8;++i){v[i]=h[i];v[i+8]=IV[i];}
 v[12]^=uint32_t(t);v[13]^=uint32_t(t>>32);if(last)v[14]=~v[14];
 for(unsigned r=0;r<10;++r){const uint8_t*s=SIGMA[r];
  G(v,0,4,8,12,m[s[0]],m[s[1]]);G(v,1,5,9,13,m[s[2]],m[s[3]]);G(v,2,6,10,14,m[s[4]],m[s[5]]);G(v,3,7,11,15,m[s[6]],m[s[7]]);
  G(v,0,5,10,15,m[s[8]],m[s[9]]);G(v,1,6,11,12,m[s[10]],m[s[11]]);G(v,2,7,8,13,m[s[12]],m[s[13]]);G(v,3,4,9,14,m[s[14]],m[s[15]]);
 }
 for(unsigned i=0;i<8;++i) h[i]^=v[i]^v[i+8];
 SecureZero(m,sizeof(m)); SecureZero(v,sizeof(v));
}
}
CryptoResult BLAKE2s256(const uint8_t*in,size_t n,uint8_t*out,size_t outn){
 if((!in&&n)||!out) return CryptoResult::InvalidArgument;
 if(outn<32) return CryptoResult::BufferTooSmall;
 uint32_t h[8];for(unsigned i=0;i<8;++i)h[i]=IV[i];h[0]^=0x01010020u;
 uint64_t count=0;size_t off=0;
 while(n-off>64){Compress(h,in+off,count+64,false);count+=64;off+=64;}
 uint8_t last[64]={0};size_t rem=n-off;for(size_t i=0;i<rem;++i)last[i]=in[off+i];count+=rem;Compress(h,last,count,true);
 for(unsigned i=0;i<8;++i) Store32(out+4*i,h[i]);
 SecureZero(h,sizeof(h)); SecureZero(last,sizeof(last)); return CryptoResult::Success;
}
CryptoResult RegisterBLAKE2s256(){static const AlgorithmDescriptor d={"BLAKE2s-256",1,AlgorithmType::Hash,kBLAKE2sDigestSize,&BLAKE2s256};return RegisterAlgorithm(&d);}
} }
