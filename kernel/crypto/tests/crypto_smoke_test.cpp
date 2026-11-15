#include "../core/crypto.h"
#include "../security/secure_memory.h"
#include "../hash/sha256.h"
#include "../hash/sha512.h"
#include "../hash/sha3.h"
#include "../hash/blake2s.h"
#include "../cipher/chacha20.h"
#include "../cipher/poly1305.h"
#include "../cipher/chacha20_poly1305.h"
#include "../kdf/hmac_sha256.h"
#include "../kdf/hkdf.h"
#include "../kdf/pbkdf2.h"
#include "../core/provider.h"
#include "../random/csprng.h"
#include "../keys/key_manager.h"
#include <stdio.h>

using namespace BlockOS::Crypto;
static int Hex(const char* s, uint8_t* out, size_t cap) {
    size_t n=0; while(s[0]&&s[1]) { if(n>=cap)return -1; unsigned v=0;
        for(unsigned i=0;i<2;++i){char c=s[i];unsigned x=(c>='0'&&c<='9')?unsigned(c-'0'):(c>='a'&&c<='f')?unsigned(c-'a'+10):(c>='A'&&c<='F')?unsigned(c-'A'+10):99u;if(x>15)return -1;v=(v<<4)|x;}
        out[n++]=uint8_t(v);s+=2;
    } return s[0]? -1 : int(n);
}
static bool Check(const char* name,const uint8_t* got,const char* expected_hex,size_t len){uint8_t expected[256];int n=Hex(expected_hex,expected,sizeof(expected));bool ok=n==int(len)&&ConstantTimeEqual(got,expected,len);if(!ok)printf("FAIL: %s\n",name);SecureZero(expected,sizeof(expected));return ok;}
static bool TestHashes(){
 const uint8_t abc[]={'a','b','c'};uint8_t out[64];bool ok=true;
 ok &= SHA256(abc,3,out,64)==CryptoResult::Success&&Check("SHA-256",out,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",32);
 ok &= SHA512(abc,3,out,64)==CryptoResult::Success&&Check("SHA-512",out,"ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f",64);
 ok &= SHA3_256(abc,3,out,64)==CryptoResult::Success&&Check("SHA3-256",out,"3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532",32);
 ok &= BLAKE2s256(abc,3,out,64)==CryptoResult::Success&&Check("BLAKE2s-256",out,"508c5e8c327c14e2e1a72ba34eeb452f37458b209ed63a294d999b4c86675982",32);
 SecureZero(out,sizeof(out));return ok;
}
static bool TestPoly1305(){uint8_t key[32],tag[16];int n=Hex("85d6be7857556d337f4452fe42d506a80103808afb0db2fd4abff6af4149f51b",key,sizeof(key));const char msg[]="Cryptographic Forum Research Group";
 return n==32&&Poly1305(key,reinterpret_cast<const uint8_t*>(msg),sizeof(msg)-1,tag)==CryptoResult::Success&&Check("Poly1305",tag,"a8061dc1305136c6c22b8baf0c0127a9",16);
}
static bool TestChaCha20(){uint8_t key[32],nonce[12],zero[64]={0},out[64];for(unsigned i=0;i<32;++i)key[i]=uint8_t(i);Hex("000000090000004a00000000",nonce,sizeof(nonce));
 bool ok=ChaCha20Xor(key,nonce,1,zero,out,64)==CryptoResult::Success&&Check("ChaCha20",out,"10f1e7e4d13b5915500fdd1fa32071c4c7d1f4c733c068030422aa9ac3d46c4e" "d2826446079faa0914c2d705d98b02a2b5129cd1de164eb9cbd083e8a2503c4e",64);
 SecureZero(key,sizeof(key));SecureZero(out,sizeof(out));return ok;
}
static bool TestKdf(){uint8_t out[64];const uint8_t pw[]="password",salt[]="salt";
 bool ok=PBKDF2_HMAC_SHA256(pw,8,salt,4,1,out,32)==CryptoResult::Success&&Check("PBKDF2-HMAC-SHA256",out,"120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b",32);
 uint8_t ikm[22];for(unsigned i=0;i<22;++i)ikm[i]=0x0b;uint8_t hsalt[13],info[10];for(unsigned i=0;i<13;++i)hsalt[i]=uint8_t(i);for(unsigned i=0;i<10;++i)info[i]=uint8_t(0xf0+i);
 ok &= HKDFSHA256(hsalt,13,ikm,22,info,10,out,42)==CryptoResult::Success&&Check("HKDF-SHA256",out,"3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865",42);
 SecureZero(out,sizeof(out));return ok;
}
static bool TestAead(){
 uint8_t key[32],nonce[12],aad[12],pt[256],ct[256],tag[16],recovered[256];
 Hex("1c9240a5eb55d38af333888604f6b5f0" "919f1f2d6e4b5c7a8f6d3e2c1b0a0908",key,32);
 Hex("000000000102030405060708",nonce,12);Hex("f33388860000000000004e91",aad,12);
 const char* message="Internet-Drafts are draft documents valid for a maximum of six months and may be updated, replaced, or obsoleted by other documents at any time.";
 size_t len=0;while(message[len])++len;for(size_t i=0;i<len;++i)pt[i]=uint8_t(message[i]);
 bool ok=ChaCha20Poly1305Encrypt(key,nonce,aad,sizeof(aad),pt,len,ct,tag)==CryptoResult::Success;
 ok &= Check("ChaCha20-Poly1305 tag",tag,"eda3c922fb27c7498e98c7965f52bec8",16);
 ok &= Check("ChaCha20-Poly1305 ciphertext",ct,"3dd2abe0efa9001b607d7c1552c66b944c5f2e8a2463287cdab1029584f1c0d817c39be2c7179086a4f79aa288d648f62cf3e1f909d7c9987b171add50191b1a85246a420c29576db7e62453c73e8696f8a8b612cdbd823ee9d771b53f5b8b49b04af5f429a55ec7d14052279ee3caac20ce068094a119d2429ed87df316de499d2631f49d2ec638c6b8f06832403f5d",len);
 ok &= ChaCha20Poly1305Decrypt(key,nonce,aad,sizeof(aad),ct,len,tag,recovered)==CryptoResult::Success&&ConstantTimeEqual(pt,recovered,len);
 tag[0]^=1;ok &= ChaCha20Poly1305Decrypt(key,nonce,aad,sizeof(aad),ct,len,tag,recovered)==CryptoResult::VerificationFailed;
 for(size_t i=0;i<len;++i)ok &= recovered[i]==0;
 if(!ok)printf("FAIL: ChaCha20-Poly1305 authentication/vector\n");
 SecureZero(key,sizeof(key));SecureZero(pt,sizeof(pt));SecureZero(ct,sizeof(ct));SecureZero(recovered,sizeof(recovered));return ok;
}

static bool TestHmac(){
    uint8_t key[20],out[32];for(unsigned i=0;i<20;++i)key[i]=0x0b;
    const uint8_t message[]={'H','i',' ','T','h','e','r','e'};
    bool ok=HMACSHA256(key,sizeof(key),message,sizeof(message),out)==CryptoResult::Success;
    ok &= Check("HMAC-SHA-256",out,"b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7",32);
    SecureZero(key,sizeof(key));SecureZero(out,sizeof(out));return ok;
}

static bool TestRegistryAndStreaming(){
    const uint8_t a[]={'a'};const uint8_t bc[]={'b','c'};uint8_t digest[64];
    SHA256Context context;SHA256Init(&context);bool ok=SHA256Update(&context,a,sizeof(a))==CryptoResult::Success;
    ok &= SHA256Update(&context,bc,sizeof(bc))==CryptoResult::Success;
    ok &= SHA256Final(&context,digest,sizeof(digest))==CryptoResult::Success;
    ok &= Check("SHA-256 streaming",digest,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",32);
    ok &= Hash("SHA-512",reinterpret_cast<const uint8_t*>("abc"),3,digest,sizeof(digest))==CryptoResult::Success;
    ok &= Check("registry SHA-512",digest,"ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f",64);
    ok &= Hash("no-such-hash",a,1,digest,sizeof(digest))==CryptoResult::NotSupported;
    SecureZero(digest,sizeof(digest));return ok;
}

static uint8_t g_test_entropy_byte=1;
static CryptoResult TestEntropyProvider(uint8_t* out,size_t n){if(!out&&n)return CryptoResult::InvalidArgument;for(size_t i=0;i<n;++i)out[i]=g_test_entropy_byte++;return CryptoResult::Success;}
static CryptoResult InspectKey(const uint8_t* key,size_t n,void* context){
    if(!key||!context) return CryptoResult::InvalidArgument;
    uint8_t* result=static_cast<uint8_t*>(context);
    result[0]=uint8_t(n); result[1]=key[0]; return CryptoResult::Success;
}
static bool TestRandomAndKeys(){
    static const RandomProvider provider={"test-only-deterministic-entropy",1,&TestEntropyProvider,true};
    if(CSPRNGInitialize()!=CryptoResult::EntropyUnavailable)return false;
    if(RegisterRandomProvider(&provider)!=CryptoResult::Success)return false;
    if(CSPRNGInitialize()!=CryptoResult::Success)return false;
    uint8_t a[32],b[32];bool ok=GenerateRandom(a,sizeof(a))==CryptoResult::Success&&GenerateRandom(b,sizeof(b))==CryptoResult::Success;
    ok &= !ConstantTimeEqual(a,b,sizeof(a));
    KeyHandle handle={0};ok &= GenerateVolatileKey(32,KeyUsageEncrypt|KeyUsageDecrypt,&handle)==CryptoResult::Success;
    uint8_t meta[2]={0};ok &= UseVolatileKey(handle,KeyUsageEncrypt,&InspectKey,meta)==CryptoResult::Success&&meta[0]==32;
    ok &= UseVolatileKey(handle,KeyUsageSign,&InspectKey,meta)==CryptoResult::PermissionDenied;
    ok &= DestroyVolatileKey(handle)==CryptoResult::Success;
    SecureZero(a,sizeof(a));SecureZero(b,sizeof(b));return ok;
}

int main(){
 if(Initialize()!=CryptoResult::Success){puts("Crypto initialization failed");return 1;}
 bool ok=true; ok=TestHashes()&&ok; ok=TestPoly1305()&&ok; ok=TestChaCha20()&&ok; ok=TestKdf()&&ok; ok=TestHmac()&&ok; ok=TestAead()&&ok; ok=TestRegistryAndStreaming()&&ok; ok=TestRandomAndKeys()&&ok;
 Shutdown();if(!ok)return 2;puts("BlockOS Crypto known-answer tests: PASS");return 0;
}
