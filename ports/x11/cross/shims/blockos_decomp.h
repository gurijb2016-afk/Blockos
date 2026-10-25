#ifndef BLOCKOS_DECOMP_H
#define BLOCKOS_DECOMP_H
#include <stddef.h>
#include <stdint.h>

/* Each call decodes the whole input into a malloc'd buffer (free with free()).
 * Returns 0 on success, negative on malformed input. */
int blockos_inflate_raw(const uint8_t *in, size_t inlen, uint8_t **out, size_t *outlen);
int blockos_gunzip(const uint8_t *in, size_t inlen, uint8_t **out, size_t *outlen);
int blockos_bunzip2(const uint8_t *in, size_t inlen, uint8_t **out, size_t *outlen);

#endif
