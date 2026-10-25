#ifndef BLOCKOS_ENDIAN_H
#define BLOCKOS_ENDIAN_H
#include <stdint.h>
#define __LITTLE_ENDIAN 1234
#define __BIG_ENDIAN    4321
#define __PDP_ENDIAN    3412
#define __BYTE_ORDER    __LITTLE_ENDIAN
#define LITTLE_ENDIAN   __LITTLE_ENDIAN
#define BIG_ENDIAN      __BIG_ENDIAN
#define PDP_ENDIAN      __PDP_ENDIAN
#define BYTE_ORDER      __BYTE_ORDER
static inline uint16_t htobe16(uint16_t x) { return (uint16_t)((x << 8) | (x >> 8)); }
static inline uint32_t htobe32(uint32_t x) { return __builtin_bswap32(x); }
static inline uint64_t htobe64(uint64_t x) { return __builtin_bswap64(x); }
#define be16toh htobe16
#define be32toh htobe32
#define be64toh htobe64
#define htole16(x) ((uint16_t)(x))
#define htole32(x) ((uint32_t)(x))
#define htole64(x) ((uint64_t)(x))
#define le16toh htole16
#define le32toh htole32
#define le64toh htole64
#endif
