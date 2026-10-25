/* SHA-1 (FIPS 180-1) with the libmd-style API used by X's os/xsha1.c. */
#include "sha1.h"
#include <string.h>

#define ROL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

static void sha1_block(uint32_t st[5], const unsigned char b[64]) {
    uint32_t w[80];
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)b[4*i] << 24) | ((uint32_t)b[4*i+1] << 16) | ((uint32_t)b[4*i+2] << 8) | b[4*i+3];
    for (int i = 16; i < 80; i++) w[i] = ROL(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
    uint32_t a = st[0], bb = st[1], c = st[2], d = st[3], e = st[4];
    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20)      { f = (bb & c) | (~bb & d); k = 0x5A827999; }
        else if (i < 40) { f = bb ^ c ^ d;           k = 0x6ED9EBA1; }
        else if (i < 60) { f = (bb & c) | (bb & d) | (c & d); k = 0x8F1BBCDC; }
        else             { f = bb ^ c ^ d;           k = 0xCA62C1D6; }
        uint32_t t = ROL(a, 5) + f + e + k + w[i];
        e = d; d = c; c = ROL(bb, 30); bb = a; a = t;
    }
    st[0] += a; st[1] += bb; st[2] += c; st[3] += d; st[4] += e;
}

void SHA1Init(SHA1_CTX *ctx) {
    ctx->state[0] = 0x67452301; ctx->state[1] = 0xEFCDAB89; ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476; ctx->state[4] = 0xC3D2E1F0;
    ctx->count = 0;
}

void SHA1Update(SHA1_CTX *ctx, const void *data, size_t len) {
    const unsigned char *p = data;
    size_t used = (size_t)(ctx->count & 63);
    ctx->count += len;
    if (used) {
        size_t take = 64 - used;
        if (take > len) take = len;
        memcpy(ctx->buffer + used, p, take);
        p += take; len -= take; used += take;
        if (used == 64) { sha1_block(ctx->state, ctx->buffer); used = 0; }
    }
    while (len >= 64) { sha1_block(ctx->state, p); p += 64; len -= 64; }
    if (len) memcpy(ctx->buffer, p, len);
}

void SHA1Final(unsigned char digest[20], SHA1_CTX *ctx) {
    uint64_t bits = ctx->count * 8;
    unsigned char pad = 0x80;
    SHA1Update(ctx, &pad, 1);
    unsigned char zero = 0;
    while ((ctx->count & 63) != 56) SHA1Update(ctx, &zero, 1);
    unsigned char lenb[8];
    for (int i = 0; i < 8; i++) lenb[i] = (unsigned char)(bits >> (56 - 8 * i));
    SHA1Update(ctx, lenb, 8);
    for (int i = 0; i < 5; i++) {
        digest[4*i]   = (unsigned char)(ctx->state[i] >> 24);
        digest[4*i+1] = (unsigned char)(ctx->state[i] >> 16);
        digest[4*i+2] = (unsigned char)(ctx->state[i] >> 8);
        digest[4*i+3] = (unsigned char)ctx->state[i];
    }
    memset(ctx, 0, sizeof(*ctx));
}
