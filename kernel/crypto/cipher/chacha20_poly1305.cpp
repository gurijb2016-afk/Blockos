#include "chacha20_poly1305.h"
#include "chacha20.h"
#include "poly1305.h"
#include "../security/secure_memory.h"
namespace BlockOS { namespace Crypto {
namespace {
void Store64LE(uint8_t*p,uint64_t x){for(unsigned i=0;i<8;++i)p[i]=uint8_t(x>>(8u*i));}
CryptoResult ComputeTag(const uint8_t key[32],const uint8_t nonce[12],const uint8_t*aad,size_t aadn,
                        const uint8_t*ct,size_t ctn,uint8_t tag[16]){
 uint8_t block[64],lengths[16],zero[64]={0};
 CryptoResult r=ChaCha20Xor(key,nonce,0,zero,block,64);
 if(r!=CryptoResult::Success){SecureZero(block,sizeof(block));return r;}
 Poly1305Context p;Poly1305Init(&p,block);
 r=Poly1305Update(&p,aad,aadn);
 if(r==CryptoResult::Success && (aadn&15u))r=Poly1305Update(&p,zero,16-(aadn&15u));
 if(r==CryptoResult::Success)r=Poly1305Update(&p,ct,ctn);
 if(r==CryptoResult::Success && (ctn&15u))r=Poly1305Update(&p,zero,16-(ctn&15u));
 Store64LE(lengths,static_cast<uint64_t>(aadn));Store64LE(lengths+8,static_cast<uint64_t>(ctn));
 if(r==CryptoResult::Success)r=Poly1305Update(&p,lengths,sizeof(lengths));
 if(r==CryptoResult::Success)r=Poly1305Final(&p,tag);
 SecureZero(&p,sizeof(p));SecureZero(block,sizeof(block));SecureZero(lengths,sizeof(lengths));return r;
}
bool ValidLength(size_t n){return static_cast<uint64_t>(n)<=(uint64_t(0xffffffffu)*64u);}
}
CryptoResult ChaCha20Poly1305Encrypt(const uint8_t key[32],const uint8_t nonce[12],const uint8_t*aad,size_t aadn,
 const uint8_t*pt,size_t ptn,uint8_t*ct,uint8_t tag[16]){
 if(!key||!nonce||(!aad&&aadn)||(!pt&&ptn)||(!ct&&ptn)||!tag)return CryptoResult::InvalidArgument;
 if(!ValidLength(ptn))return CryptoResult::InvalidArgument;
 CryptoResult r=ChaCha20Xor(key,nonce,1,pt,ct,ptn);if(r!=CryptoResult::Success)return r;
 r=ComputeTag(key,nonce,aad,aadn,ct,ptn,tag);if(r!=CryptoResult::Success&&ptn)SecureZero(ct,ptn);return r;
}
CryptoResult ChaCha20Poly1305Decrypt(const uint8_t key[32],const uint8_t nonce[12],const uint8_t*aad,size_t aadn,
 const uint8_t*ct,size_t ctn,const uint8_t tag[16],uint8_t*pt){
 if(!key||!nonce||(!aad&&aadn)||(!ct&&ctn)||(!pt&&ctn)||!tag)return CryptoResult::InvalidArgument;
 if(!ValidLength(ctn))return CryptoResult::InvalidArgument;
 uint8_t expected[16];CryptoResult r=ComputeTag(key,nonce,aad,aadn,ct,ctn,expected);
 if(r!=CryptoResult::Success){SecureZero(expected,sizeof(expected));return r;}
 const bool valid=ConstantTimeEqual(expected,tag,sizeof(expected));SecureZero(expected,sizeof(expected));
 if(!valid){if(ctn)SecureZero(pt,ctn);return CryptoResult::VerificationFailed;}
 return ChaCha20Xor(key,nonce,1,ct,pt,ctn);
}
} }
