#include "csprng.h"
#include "../core/provider.h"
#include "../kdf/hmac_sha256.h"
#include "../security/secure_memory.h"
namespace BlockOS { namespace Crypto {
namespace {
struct HMACDRBGState { uint8_t key[32]; uint8_t value[32]; uint64_t reseed_counter; bool initialized; };
HMACDRBGState g_state = {};
constexpr size_t kSeedBytes = 48;
constexpr size_t kMaxRequest = 65536;
constexpr uint64_t kReseedInterval = 1048576;
CryptoResult Hmac(const uint8_t key[32], const uint8_t* a, size_t an,
                  uint8_t separator, const uint8_t* b, size_t bn, uint8_t out[32]) {
    HMACSHA256Context h; HMACSHA256Init(&h,key,32);
    CryptoResult r=HMACSHA256Update(&h,a,an);
    if(r==CryptoResult::Success)r=HMACSHA256Update(&h,&separator,1);
    if(r==CryptoResult::Success)r=HMACSHA256Update(&h,b,bn);
    if(r==CryptoResult::Success)r=HMACSHA256Final(&h,out);else SecureZero(&h,sizeof(h));
    return r;
}
CryptoResult Update(const uint8_t* provided,size_t n) {
    uint8_t next[32];CryptoResult r=Hmac(g_state.key,g_state.value,32,0x00,provided,n,next);
    if(r==CryptoResult::Success){for(unsigned i=0;i<32;++i)g_state.key[i]=next[i];r=HMACSHA256(g_state.key,32,g_state.value,32,g_state.value);}
    if(r==CryptoResult::Success&&n){r=Hmac(g_state.key,g_state.value,32,0x01,provided,n,next);if(r==CryptoResult::Success){for(unsigned i=0;i<32;++i)g_state.key[i]=next[i];r=HMACSHA256(g_state.key,32,g_state.value,32,g_state.value);}}
    SecureZero(next,sizeof(next));return r;
}
CryptoResult Instantiate(const uint8_t* seed,size_t n) {
    if(!seed||n<32)return CryptoResult::InvalidArgument;
    for(unsigned i=0;i<32;++i){g_state.key[i]=0;g_state.value[i]=1;}
    g_state.initialized=true;g_state.reseed_counter=1;
    CryptoResult r=Update(seed,n);if(r!=CryptoResult::Success){SecureZero(&g_state,sizeof(g_state));return r;}return CryptoResult::Success;
}
}
CryptoResult CSPRNGInitialize(){
    if(g_state.initialized)return CryptoResult::Success;
    uint8_t seed[kSeedBytes];CryptoResult r=CollectPlatformEntropy(seed,sizeof(seed));
    if(r==CryptoResult::Success)r=Instantiate(seed,sizeof(seed));
    SecureZero(seed,sizeof(seed));return r;
}
CryptoResult CSPRNGReseed(const uint8_t* entropy,size_t length){
    if(!entropy||length<32)return CryptoResult::InvalidArgument;
    if(!g_state.initialized)return Instantiate(entropy,length);
    CryptoResult r=Update(entropy,length);if(r==CryptoResult::Success)g_state.reseed_counter=1;return r;
}
CryptoResult CSPRNGFill(uint8_t* output,size_t length){
    if(length==0) return CryptoResult::Success;
    if(!output) return CryptoResult::InvalidArgument;
    if(!g_state.initialized) return CryptoResult::NotInitialized;
    if(length>kMaxRequest) return CryptoResult::BufferTooSmall;
    if(g_state.reseed_counter>kReseedInterval)return CryptoResult::EntropyUnavailable;
    size_t done=0;uint8_t next[32];CryptoResult r=CryptoResult::Success;
    while(done<length&&r==CryptoResult::Success){r=HMACSHA256(g_state.key,32,g_state.value,32,next);if(r!=CryptoResult::Success)break;
        for(unsigned i=0;i<32;++i) g_state.value[i]=next[i];
        size_t take=length-done; if(take>32)take=32;
        for(size_t i=0;i<take;++i) output[done+i]=g_state.value[i];
        done+=take;
    }
    if(r==CryptoResult::Success) r=Update(nullptr,0);
    if(r==CryptoResult::Success) ++g_state.reseed_counter;
    if(r!=CryptoResult::Success&&length) SecureZero(output,length);
    SecureZero(next,sizeof(next)); return r;
}
bool CSPRNGIsInitialized(){return g_state.initialized;}
void CSPRNGShutdown(){SecureZero(&g_state,sizeof(g_state));}
} }
