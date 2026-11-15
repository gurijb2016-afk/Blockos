#include "key_manager.h"
#include "../core/crypto.h"
#include "../security/secure_memory.h"
namespace BlockOS { namespace Crypto {
namespace {
constexpr size_t kSlots=64;
constexpr uint32_t kKnownUsage=KeyUsageEncrypt|KeyUsageDecrypt|KeyUsageSign|KeyUsageVerify|KeyUsageDerive|KeyUsageAuthenticate;
struct Record { uint64_t id; size_t length; uint32_t usage; bool active; uint8_t material[kMaxVolatileKeyBytes]; };
Record g_records[kSlots]={};uint64_t g_next_id=1;
Record* Find(KeyHandle h){
 if(!h.value)return nullptr;
 for(size_t i=0;i<kSlots;++i)if(g_records[i].active&&g_records[i].id==h.value)return &g_records[i];
 return nullptr;
}
uint64_t NextId(){
 for(size_t attempt=0;attempt<=kSlots;++attempt){
  uint64_t candidate=g_next_id++;
  if(g_next_id==0)g_next_id=1;
  if(candidate==0)continue;
  bool used=false;
  for(size_t i=0;i<kSlots;++i)if(g_records[i].active&&g_records[i].id==candidate){used=true;break;}
  if(!used)return candidate;
 }
 return 0;
}
CryptoResult Allocate(const uint8_t*key,size_t n,uint32_t usage,KeyHandle*out){
 if(!key||!out||n==0||n>kMaxVolatileKeyBytes||usage==0||(usage&~kKnownUsage))return CryptoResult::InvalidArgument;
 for(size_t i=0;i<kSlots;++i)if(!g_records[i].active){Record&r=g_records[i];r.id=NextId();if(!r.id)return CryptoResult::InternalError;r.length=n;r.usage=usage;r.active=true;for(size_t j=0;j<n;++j)r.material[j]=key[j];for(size_t j=n;j<kMaxVolatileKeyBytes;++j)r.material[j]=0;out->value=r.id;return CryptoResult::Success;}
 return CryptoResult::RegistryFull;
}
}
CryptoResult ImportVolatileKey(const uint8_t*material,size_t n,uint32_t usage,KeyHandle*handle){return Allocate(material,n,usage,handle);}
CryptoResult GenerateVolatileKey(size_t n,uint32_t usage,KeyHandle*handle){
 if(!handle||n==0||n>kMaxVolatileKeyBytes||usage==0||(usage&~kKnownUsage))return CryptoResult::InvalidArgument;
 uint8_t material[kMaxVolatileKeyBytes];CryptoResult r=GenerateRandom(material,n);if(r==CryptoResult::Success)r=Allocate(material,n,usage,handle);SecureZero(material,sizeof(material));return r;
}
CryptoResult UseVolatileKey(KeyHandle handle,uint32_t required,KeyUseCallback callback,void*context){
 if(!callback||required==0||(required&~kKnownUsage)) return CryptoResult::InvalidArgument;
 Record*r=Find(handle);
 if(!r) return CryptoResult::NotFound;
 if((r->usage&required)!=required) return CryptoResult::PermissionDenied;
 return callback(r->material,r->length,context);
}
CryptoResult GetVolatileKeyMetadata(KeyHandle handle,KeyMetadata*metadata){if(!metadata)return CryptoResult::InvalidArgument;Record*r=Find(handle);if(!r)return CryptoResult::NotFound;metadata->material_size=r->length;metadata->usage=r->usage;metadata->active=true;return CryptoResult::Success;}
CryptoResult DestroyVolatileKey(KeyHandle handle){Record*r=Find(handle);if(!r)return CryptoResult::NotFound;SecureZero(r,sizeof(*r));return CryptoResult::Success;}
void DestroyAllVolatileKeys(){for(size_t i=0;i<kSlots;++i)SecureZero(&g_records[i],sizeof(g_records[i]));}
} }
