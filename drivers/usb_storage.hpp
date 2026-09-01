#pragma once
#include <stdint.h>
#include <stddef.h>
namespace usb::storage {
struct Transport { bool (*control)(void*,uint8_t,uint8_t,uint16_t,uint16_t,void*,uint16_t); bool (*bulk_in)(void*,uint8_t,void*,uint32_t); bool (*bulk_out)(void*,uint8_t,const void*,uint32_t); void* ctx; uint8_t ep_in,ep_out; };
#pragma pack(push,1)
struct CBW {uint32_t sig;uint32_t tag;uint32_t xfer;uint8_t flags;uint8_t lun;uint8_t len;uint8_t cdb[16];};
struct CSW {uint32_t sig;uint32_t tag;uint32_t residue;uint8_t status;};
#pragma pack(pop)
bool inquiry(const Transport*,uint8_t lun,char*vendor,size_t vlen,char*product,size_t plen);
bool read10(const Transport*,uint8_t lun,uint32_t lba,uint16_t blocks,uint32_t block_size,void* buffer);
}
