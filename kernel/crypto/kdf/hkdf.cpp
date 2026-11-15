#include "hkdf.h"
#include "hmac_sha256.h"
#include "../security/secure_memory.h"
namespace BlockOS { namespace Crypto {
CryptoResult HKDFSHA256(const uint8_t*salt,size_t saltn,const uint8_t*ikm,size_t ikmn,const uint8_t*info,size_t infon,uint8_t*out,size_t outn){
 if((!salt&&saltn)||(!ikm&&ikmn)||(!info&&infon)||(!out&&outn))return CryptoResult::InvalidArgument;
 if(outn>255u*32u)return CryptoResult::BufferTooSmall;
 uint8_t zero_salt[32]={0},prk[32],t[32];if(!salt){salt=zero_salt;saltn=sizeof(zero_salt);}
 CryptoResult r=HMACSHA256(salt,saltn,ikm,ikmn,prk);size_t done=0,tlen=0;uint8_t counter=1;
 while(r==CryptoResult::Success&&done<outn){HMACSHA256Context h;HMACSHA256Init(&h,prk,sizeof(prk));if(tlen)r=HMACSHA256Update(&h,t,tlen);if(r==CryptoResult::Success)r=HMACSHA256Update(&h,info,infon);if(r==CryptoResult::Success)r=HMACSHA256Update(&h,&counter,1);if(r==CryptoResult::Success)r=HMACSHA256Final(&h,t);else SecureZero(&h,sizeof(h));
  if(r!=CryptoResult::Success) break;
  tlen=32; size_t take=outn-done; if(take>32)take=32;
  for(size_t i=0;i<take;++i) out[done+i]=t[i];
  done+=take; ++counter;
 }
 SecureZero(zero_salt,sizeof(zero_salt));SecureZero(prk,sizeof(prk));SecureZero(t,sizeof(t));return r;
}
} }
