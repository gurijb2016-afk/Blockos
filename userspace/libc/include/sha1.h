#ifndef BLOCKOS_SHA1_H
#define BLOCKOS_SHA1_H
#include <stdint.h>
#include <stddef.h>
typedef struct { uint32_t state[5]; uint64_t count; unsigned char buffer[64]; } SHA1_CTX;
void SHA1Init(SHA1_CTX *ctx);
void SHA1Update(SHA1_CTX *ctx, const void *data, size_t len);
void SHA1Final(unsigned char digest[20], SHA1_CTX *ctx);
#endif
