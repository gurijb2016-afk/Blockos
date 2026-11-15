#include "hmac_sha256.h"
#include "../security/secure_memory.h"
namespace BlockOS { namespace Crypto {
void HMACSHA256Init(HMACSHA256Context*c,const uint8_t*key,size_t keyn){
 if(!c) return;
 uint8_t key_block[64]={0},hashed[32]={0};
 if(!key && keyn){SecureZero(c,sizeof(*c));c->finalized=true;return;}
 if(keyn>64 && key){SHA256(key,keyn,hashed,sizeof(hashed));for(unsigned i=0;i<32;++i)key_block[i]=hashed[i];}
 else if(keyn&&key){for(size_t i=0;i<keyn && i<64;++i)key_block[i]=key[i];}
 for(unsigned i=0;i<64;++i)c->outer_pad[i]=uint8_t(key_block[i]^0x5c);
 uint8_t inner_pad[64];for(unsigned i=0;i<64;++i)inner_pad[i]=uint8_t(key_block[i]^0x36);
 SHA256Init(&c->inner);SHA256Update(&c->inner,inner_pad,sizeof(inner_pad));c->finalized=false;
 SecureZero(key_block,sizeof(key_block));SecureZero(hashed,sizeof(hashed));SecureZero(inner_pad,sizeof(inner_pad));
}
CryptoResult HMACSHA256Update(HMACSHA256Context*c,const uint8_t*d,size_t n){if(!c||c->finalized||(!d&&n))return CryptoResult::InvalidArgument;return SHA256Update(&c->inner,d,n);}
CryptoResult HMACSHA256Final(HMACSHA256Context*c,uint8_t out[32]){
 if(!c||!out||c->finalized) return CryptoResult::InvalidArgument;
 uint8_t inner[32]; CryptoResult r=SHA256Final(&c->inner,inner,sizeof(inner));
 if(r==CryptoResult::Success){SHA256Context outer;SHA256Init(&outer);SHA256Update(&outer,c->outer_pad,64);SHA256Update(&outer,inner,32);r=SHA256Final(&outer,out,32);SecureZero(&outer,sizeof(outer));}
 SecureZero(inner,sizeof(inner));SecureZero(c,sizeof(*c));c->finalized=true;return r;
}
CryptoResult HMACSHA256(const uint8_t*key,size_t keyn,const uint8_t*data,size_t n,uint8_t out[32]){
 if((!key&&keyn)||(!data&&n)||!out) return CryptoResult::InvalidArgument;
 HMACSHA256Context c; HMACSHA256Init(&c,key,keyn);
 CryptoResult r=HMACSHA256Update(&c,data,n);
 if(r==CryptoResult::Success) r=HMACSHA256Final(&c,out);
 SecureZero(&c,sizeof(c)); return r;
}
} }
