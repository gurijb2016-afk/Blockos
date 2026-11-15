#include "chacha20.h"
#include "../security/secure_memory.h"
namespace BlockOS { namespace Crypto {
namespace {
inline uint32_t Load32(const uint8_t*p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
inline void Store32(uint8_t*p,uint32_t x){p[0]=uint8_t(x);p[1]=uint8_t(x>>8);p[2]=uint8_t(x>>16);p[3]=uint8_t(x>>24);}
inline uint32_t Rotl(uint32_t x,unsigned n){return(x<<n)|(x>>(32u-n));}
inline void QR(uint32_t&a,uint32_t&b,uint32_t&c,uint32_t&d){a+=b;d^=a;d=Rotl(d,16);c+=d;b^=c;b=Rotl(b,12);a+=b;d^=a;d=Rotl(d,8);c+=d;b^=c;b=Rotl(b,7);}
void Block(const uint8_t key[32],const uint8_t nonce[12],uint32_t counter,uint8_t out[64]){
 uint32_t s[16]={0x61707865u,0x3320646eu,0x79622d32u,0x6b206574u};
 for(unsigned i=0;i<8;++i) s[4+i]=Load32(key+4*i);
 s[12]=counter; s[13]=Load32(nonce); s[14]=Load32(nonce+4); s[15]=Load32(nonce+8);
 uint32_t x[16];for(unsigned i=0;i<16;++i)x[i]=s[i];
 for(unsigned i=0;i<10;++i){QR(x[0],x[4],x[8],x[12]);QR(x[1],x[5],x[9],x[13]);QR(x[2],x[6],x[10],x[14]);QR(x[3],x[7],x[11],x[15]);QR(x[0],x[5],x[10],x[15]);QR(x[1],x[6],x[11],x[12]);QR(x[2],x[7],x[8],x[13]);QR(x[3],x[4],x[9],x[14]);}
 for(unsigned i=0;i<16;++i) Store32(out+4*i,x[i]+s[i]);
 SecureZero(s,sizeof(s)); SecureZero(x,sizeof(x));
}
}
CryptoResult ChaCha20Xor(const uint8_t key[32],const uint8_t nonce[12],uint32_t counter,const uint8_t*in,uint8_t*out,size_t n){
 if(!key||!nonce||(!in&&n)||(!out&&n))return CryptoResult::InvalidArgument;
 const uint64_t blocks=static_cast<uint64_t>(n/64)+(n%64?1u:0u);
 if(blocks && blocks-1u>uint64_t(0xffffffffu-counter))return CryptoResult::InvalidArgument;
 uint8_t stream[64];size_t off=0;while(off<n){Block(key,nonce,counter++,stream);size_t take=n-off;if(take>64)take=64;for(size_t i=0;i<take;++i)out[off+i]=uint8_t(in[off+i]^stream[i]);off+=take;}
 SecureZero(stream,sizeof(stream));return CryptoResult::Success;
}
} }
