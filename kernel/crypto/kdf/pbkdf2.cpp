#include "pbkdf2.h"
#include "hmac_sha256.h"
#include "../security/secure_memory.h"
namespace BlockOS { namespace Crypto {
CryptoResult PBKDF2_HMAC_SHA256(const uint8_t*pw,size_t pwn,const uint8_t*salt,size_t saltn,uint32_t iterations,uint8_t*out,size_t outn){
 if((!pw&&pwn)||(!salt&&saltn)||(!out&&outn)||iterations==0)return CryptoResult::InvalidArgument;
 const uint64_t blocks=(static_cast<uint64_t>(outn)/32u)+(outn%32u?1u:0u);if(blocks>0xffffffffULL)return CryptoResult::BufferTooSmall;
 uint8_t u[32],t[32],be[4];size_t done=0;CryptoResult r=CryptoResult::Success;
 for(uint32_t block=1;uint64_t(block)<=blocks&&r==CryptoResult::Success;++block){be[0]=uint8_t(block>>24);be[1]=uint8_t(block>>16);be[2]=uint8_t(block>>8);be[3]=uint8_t(block);
  HMACSHA256Context h;HMACSHA256Init(&h,pw,pwn);r=HMACSHA256Update(&h,salt,saltn);if(r==CryptoResult::Success)r=HMACSHA256Update(&h,be,4);if(r==CryptoResult::Success)r=HMACSHA256Final(&h,u);else SecureZero(&h,sizeof(h));
  if(r!=CryptoResult::Success) break;
  for(unsigned i=0;i<32;++i) t[i]=u[i];
  for(uint32_t j=1;j<iterations;++j){r=HMACSHA256(pw,pwn,u,sizeof(u),u);if(r!=CryptoResult::Success)break;for(unsigned i=0;i<32;++i)t[i]^=u[i];}
  if(r==CryptoResult::Success){size_t take=outn-done;if(take>32)take=32;for(size_t i=0;i<take;++i)out[done+i]=t[i];done+=take;}
 }
 SecureZero(u,sizeof(u));SecureZero(t,sizeof(t));SecureZero(be,sizeof(be));return r;
}
} }
