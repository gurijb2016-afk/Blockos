#pragma once
#include <stdint.h>
#include <stddef.h>
namespace nvme {
bool init();
bool available();
bool identify(uint8_t out_model[40], uint32_t& namespace_count);
bool identify_namespace(uint32_t nsid, uint64_t& block_count, uint32_t& block_size);
bool read(uint32_t nsid, uint64_t lba, uint16_t blocks, void* buffer, size_t block_size = 512);
bool write(uint32_t nsid, uint64_t lba, uint16_t blocks, const void* buffer, size_t block_size = 512);
}
