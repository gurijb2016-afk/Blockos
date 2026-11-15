#include "poly1305.h"
#include "../security/secure_memory.h"
namespace BlockOS { namespace Crypto {
namespace {
inline uint32_t Load32(const uint8_t*p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
inline void Store32(uint8_t*p,uint32_t x){p[0]=uint8_t(x);p[1]=uint8_t(x>>8);p[2]=uint8_t(x>>16);p[3]=uint8_t(x>>24);}
void Blocks(Poly1305Context*c,const uint8_t*m,size_t bytes,uint32_t hibit){
 const uint32_t r0=c->r[0],r1=c->r[1],r2=c->r[2],r3=c->r[3],r4=c->r[4];
 const uint32_t s1=r1*5u,s2=r2*5u,s3=r3*5u,s4=r4*5u;
 uint32_t h0=c->h[0],h1=c->h[1],h2=c->h[2],h3=c->h[3],h4=c->h[4];
 while(bytes>=16){
  h0+= Load32(m+0)&0x3ffffffu;h1+=(Load32(m+3)>>2)&0x3ffffffu;h2+=(Load32(m+6)>>4)&0x3ffffffu;
  h3+=(Load32(m+9)>>6)&0x3ffffffu;h4+=(Load32(m+12)>>8)|hibit;
  uint64_t d0=uint64_t(h0)*r0+uint64_t(h1)*s4+uint64_t(h2)*s3+uint64_t(h3)*s2+uint64_t(h4)*s1;
  uint64_t d1=uint64_t(h0)*r1+uint64_t(h1)*r0+uint64_t(h2)*s4+uint64_t(h3)*s3+uint64_t(h4)*s2;
  uint64_t d2=uint64_t(h0)*r2+uint64_t(h1)*r1+uint64_t(h2)*r0+uint64_t(h3)*s4+uint64_t(h4)*s3;
  uint64_t d3=uint64_t(h0)*r3+uint64_t(h1)*r2+uint64_t(h2)*r1+uint64_t(h3)*r0+uint64_t(h4)*s4;
  uint64_t d4=uint64_t(h0)*r4+uint64_t(h1)*r3+uint64_t(h2)*r2+uint64_t(h3)*r1+uint64_t(h4)*r0;
  uint32_t carry=uint32_t(d0>>26);h0=uint32_t(d0)&0x3ffffffu;d1+=carry;
  carry=uint32_t(d1>>26);h1=uint32_t(d1)&0x3ffffffu;d2+=carry;
  carry=uint32_t(d2>>26);h2=uint32_t(d2)&0x3ffffffu;d3+=carry;
  carry=uint32_t(d3>>26);h3=uint32_t(d3)&0x3ffffffu;d4+=carry;
  carry=uint32_t(d4>>26);h4=uint32_t(d4)&0x3ffffffu;h0+=carry*5u;
  carry=h0>>26;h0&=0x3ffffffu;h1+=carry;
  m+=16;bytes-=16;
 }
 c->h[0]=h0;c->h[1]=h1;c->h[2]=h2;c->h[3]=h3;c->h[4]=h4;
}
}
void Poly1305Init(Poly1305Context*c,const uint8_t key[32]){
 if(!c||!key)return;
 c->r[0]= Load32(key+0)&0x3ffffffu;c->r[1]=(Load32(key+3)>>2)&0x3ffff03u;c->r[2]=(Load32(key+6)>>4)&0x3ffc0ffu;
 c->r[3]=(Load32(key+9)>>6)&0x3f03fffu;c->r[4]=(Load32(key+12)>>8)&0x00fffffu;
 for(unsigned i=0;i<5;++i) c->h[i]=0;
 for(unsigned i=0;i<4;++i) c->pad[i]=Load32(key+16+4*i);
 c->leftover=0;c->finalized=false;for(unsigned i=0;i<16;++i)c->buffer[i]=0;
}
CryptoResult Poly1305Update(Poly1305Context*c,const uint8_t*m,size_t n){
 if(!c||(!m&&n)) return CryptoResult::InvalidArgument;
 if(c->finalized) return CryptoResult::PermissionDenied;
 if(c->leftover){size_t want=16-c->leftover;if(want>n)want=n;for(size_t i=0;i<want;++i)c->buffer[c->leftover+i]=m[i];m+=want;n-=want;c->leftover+=want;if(c->leftover<16)return CryptoResult::Success;Blocks(c,c->buffer,16,1u<<24);c->leftover=0;}
 if(n>=16){size_t want=n&~static_cast<size_t>(0xf);Blocks(c,m,want,1u<<24);m+=want;n-=want;}
 if(n){for(size_t i=0;i<n;++i)c->buffer[i]=m[i];c->leftover=n;}return CryptoResult::Success;
}
CryptoResult Poly1305Final(Poly1305Context*c,uint8_t tag[16]){
 if(!c||!tag) return CryptoResult::InvalidArgument;
 if(c->finalized) return CryptoResult::PermissionDenied;
 if(c->leftover){c->buffer[c->leftover]=1;for(size_t i=c->leftover+1;i<16;++i)c->buffer[i]=0;Blocks(c,c->buffer,16,0);}
 uint32_t h0=c->h[0],h1=c->h[1],h2=c->h[2],h3=c->h[3],h4=c->h[4];
 uint32_t carry=h1>>26;h1&=0x3ffffffu;h2+=carry;carry=h2>>26;h2&=0x3ffffffu;h3+=carry;carry=h3>>26;h3&=0x3ffffffu;h4+=carry;carry=h4>>26;h4&=0x3ffffffu;h0+=carry*5u;carry=h0>>26;h0&=0x3ffffffu;h1+=carry;
 uint32_t g0=h0+5u;carry=g0>>26;g0&=0x3ffffffu;uint32_t g1=h1+carry;carry=g1>>26;g1&=0x3ffffffu;uint32_t g2=h2+carry;carry=g2>>26;g2&=0x3ffffffu;uint32_t g3=h3+carry;carry=g3>>26;g3&=0x3ffffffu;uint32_t g4=h4+carry-(1u<<26);
 const uint32_t mask=(g4>>31)-1u;const uint32_t nmask=~mask;h0=(h0&nmask)|(g0&mask);h1=(h1&nmask)|(g1&mask);h2=(h2&nmask)|(g2&mask);h3=(h3&nmask)|(g3&mask);h4=(h4&nmask)|(g4&mask);
 const uint32_t w0=h0|(h1<<26);const uint32_t w1=(h1>>6)|(h2<<20);
 const uint32_t w2=(h2>>12)|(h3<<14);const uint32_t w3=(h3>>18)|(h4<<8);
 uint64_t f0=uint64_t(w0)+c->pad[0];uint64_t f1=uint64_t(w1)+c->pad[1]+(f0>>32);
 uint64_t f2=uint64_t(w2)+c->pad[2]+(f1>>32);uint64_t f3=uint64_t(w3)+c->pad[3]+(f2>>32);
 Store32(tag,uint32_t(f0));Store32(tag+4,uint32_t(f1));Store32(tag+8,uint32_t(f2));Store32(tag+12,uint32_t(f3));
 SecureZero(c,sizeof(*c));c->finalized=true;return CryptoResult::Success;
}
CryptoResult Poly1305(const uint8_t key[32],const uint8_t*m,size_t n,uint8_t tag[16]){
 if(!key||(!m&&n)||!tag) return CryptoResult::InvalidArgument;
 Poly1305Context c; Poly1305Init(&c,key);
 CryptoResult r=Poly1305Update(&c,m,n);
 if(r==CryptoResult::Success) r=Poly1305Final(&c,tag);
 SecureZero(&c,sizeof(c)); return r;
}
} }
