/* BlockOS decompressors for the X font path: gzip (RFC 1952 wrapper around
 * RFC 1951 deflate) and bzip2. Pure memory-to-memory code with no X or zlib
 * dependency, so it can be unit-tested on the host (see blockos_decomp_test.c).
 *
 * Both functions decode a complete input buffer into a malloc'd output buffer.
 * Font files are small (hundreds of KB), so whole-file decoding keeps the
 * BufFile glue trivial. Return 0 on success, negative on error. */
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "blockos_decomp.h"

/* ------------------------------------------------------------------ output */

typedef struct {
    uint8_t *buf;
    size_t len;
    size_t cap;
} outbuf;

static int out_reserve(outbuf *o, size_t extra) {
    if (o->len + extra <= o->cap) return 0;
    size_t want = o->cap ? o->cap : 4096;
    while (want < o->len + extra) {
        if (want > ((size_t)1 << 30)) return -1;
        want <<= 1;
    }
    uint8_t *nb = (uint8_t *)realloc(o->buf, want);
    if (!nb) return -1;
    o->buf = nb;
    o->cap = want;
    return 0;
}

static int out_put(outbuf *o, uint8_t c) {
    if (out_reserve(o, 1)) return -1;
    o->buf[o->len++] = c;
    return 0;
}

/* ================================================================ inflate */

typedef struct {
    const uint8_t *in;
    size_t inlen;
    size_t inpos;
    uint32_t bitbuf;
    int bitcnt;
    outbuf out;
} inflate_state;

/* Returns the next n bits (LSB first), or -1 on input exhaustion. */
static int bits(inflate_state *s, int n) {
    int32_t val = 0;
    while (s->bitcnt < n) {
        if (s->inpos >= s->inlen) return -1;
        s->bitbuf |= (uint32_t)s->in[s->inpos++] << s->bitcnt;
        s->bitcnt += 8;
    }
    val = (int32_t)(s->bitbuf & ((1u << n) - 1));
    s->bitbuf >>= n;
    s->bitcnt -= n;
    return val;
}

#define MAXBITS 15
typedef struct {
    uint16_t count[MAXBITS + 1];
    uint16_t symbol[320];
} huffman;

/* Builds a canonical Huffman table. Returns 0 if complete, >0 if incomplete
 * (allowed for single-code distance trees), <0 if over-subscribed. */
static int huff_build(huffman *h, const uint16_t *length, int n) {
    int left = 1;
    uint16_t offs[MAXBITS + 1];
    for (int len = 0; len <= MAXBITS; len++) h->count[len] = 0;
    for (int sym = 0; sym < n; sym++) h->count[length[sym]]++;
    if (h->count[0] == n) return 0; /* no codes: complete, nothing decodes */
    for (int len = 1; len <= MAXBITS; len++) {
        left <<= 1;
        left -= h->count[len];
        if (left < 0) return left;
    }
    offs[1] = 0;
    for (int len = 1; len < MAXBITS; len++) offs[len + 1] = (uint16_t)(offs[len] + h->count[len]);
    for (int sym = 0; sym < n; sym++)
        if (length[sym] != 0) h->symbol[offs[length[sym]]++] = (uint16_t)sym;
    return left;
}

/* Reads one symbol using canonical code h (bit-at-a-time, MSB of code first). */
static int huff_decode(inflate_state *s, const huffman *h) {
    int code = 0, first = 0, index = 0;
    for (int len = 1; len <= MAXBITS; len++) {
        int b = bits(s, 1);
        if (b < 0) return -1;
        code |= b;
        int count = h->count[len];
        if (code - count < first) return h->symbol[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return -2;
}

static const uint16_t len_base[29] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
};
static const uint16_t len_extra[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
};
static const uint16_t dist_base[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
    257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
    8193, 12289, 16385, 24577
};
static const uint16_t dist_extra[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
};

static int inflate_codes(inflate_state *s, const huffman *lencode, const huffman *distcode) {
    for (;;) {
        int sym = huff_decode(s, lencode);
        if (sym < 0) return -1;
        if (sym < 256) {
            if (out_put(&s->out, (uint8_t)sym)) return -2;
        } else if (sym == 256) {
            return 0;
        } else {
            sym -= 257;
            if (sym >= 29) return -3;
            int e = bits(s, len_extra[sym]);
            if (e < 0) return -1;
            size_t len = len_base[sym] + (size_t)e;
            int ds = huff_decode(s, distcode);
            if (ds < 0 || ds >= 30) return -4;
            e = bits(s, dist_extra[ds]);
            if (e < 0) return -1;
            size_t dist = dist_base[ds] + (size_t)e;
            if (dist > s->out.len) return -5;
            if (out_reserve(&s->out, len)) return -2;
            size_t from = s->out.len - dist;
            for (size_t i = 0; i < len; i++)
                s->out.buf[s->out.len++] = s->out.buf[from + i];
        }
    }
}

static int inflate_stored(inflate_state *s) {
    /* Discard the rest of the current byte, then LEN/NLEN follow byte-aligned. */
    s->bitbuf = 0;
    s->bitcnt = 0;
    if (s->inpos + 4 > s->inlen) return -1;
    uint16_t len = (uint16_t)(s->in[s->inpos] | (s->in[s->inpos + 1] << 8));
    uint16_t nlen = (uint16_t)(s->in[s->inpos + 2] | (s->in[s->inpos + 3] << 8));
    s->inpos += 4;
    if ((uint16_t)~nlen != len) return -6;
    if (s->inpos + len > s->inlen) return -1;
    if (out_reserve(&s->out, len)) return -2;
    memcpy(s->out.buf + s->out.len, s->in + s->inpos, len);
    s->out.len += len;
    s->inpos += len;
    return 0;
}

static int inflate_fixed(inflate_state *s) {
    static huffman lencode, distcode;
    static int ready = 0;
    if (!ready) {
        uint16_t lengths[288 + 30];
        int i = 0;
        for (; i < 144; i++) lengths[i] = 8;
        for (; i < 256; i++) lengths[i] = 9;
        for (; i < 280; i++) lengths[i] = 7;
        for (; i < 288; i++) lengths[i] = 8;
        huff_build(&lencode, lengths, 288);
        for (i = 0; i < 30; i++) lengths[i] = 5;
        huff_build(&distcode, lengths, 30);
        ready = 1;
    }
    return inflate_codes(s, &lencode, &distcode);
}

static int inflate_dynamic(inflate_state *s) {
    static const uint8_t order[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
    uint16_t lengths[320];
    huffman lencode, distcode;
    int nlen = bits(s, 5); if (nlen < 0) return -1; nlen += 257;
    int ndist = bits(s, 5); if (ndist < 0) return -1; ndist += 1;
    int ncode = bits(s, 4); if (ncode < 0) return -1; ncode += 4;
    if (nlen > 286 || ndist > 30) return -7;

    for (int i = 0; i < 19; i++) lengths[i] = 0;
    for (int i = 0; i < ncode; i++) {
        int v = bits(s, 3); if (v < 0) return -1;
        lengths[order[i]] = (uint16_t)v;
    }
    if (huff_build(&lencode, lengths, 19) != 0) return -8;

    int index = 0;
    while (index < nlen + ndist) {
        int sym = huff_decode(s, &lencode);
        if (sym < 0) return -1;
        if (sym < 16) {
            lengths[index++] = (uint16_t)sym;
        } else {
            uint16_t val = 0;
            int rep;
            if (sym == 16) {
                if (index == 0) return -9;
                val = lengths[index - 1];
                rep = bits(s, 2); if (rep < 0) return -1; rep += 3;
            } else if (sym == 17) {
                rep = bits(s, 3); if (rep < 0) return -1; rep += 3;
            } else {
                rep = bits(s, 7); if (rep < 0) return -1; rep += 11;
            }
            if (index + rep > nlen + ndist) return -10;
            while (rep--) lengths[index++] = val;
        }
    }
    if (lengths[256] == 0) return -11;
    int err = huff_build(&lencode, lengths, nlen);
    if (err < 0 || (err > 0 && nlen - lencode.count[0] != 1)) return -12;
    err = huff_build(&distcode, lengths + nlen, ndist);
    if (err < 0 || (err > 0 && ndist - distcode.count[0] != 1)) return -13;
    return inflate_codes(s, &lencode, &distcode);
}

/* Raw deflate (RFC 1951) from s->in[s->inpos..] into s->out. */
static int inflate_raw(inflate_state *s) {
    int last;
    do {
        last = bits(s, 1);
        if (last < 0) return -1;
        int type = bits(s, 2);
        if (type < 0) return -1;
        int rc;
        if (type == 0) rc = inflate_stored(s);
        else if (type == 1) rc = inflate_fixed(s);
        else if (type == 2) rc = inflate_dynamic(s);
        else return -14;
        if (rc) return rc;
    } while (!last);
    return 0;
}

int blockos_inflate_raw(const uint8_t *in, size_t inlen, uint8_t **out, size_t *outlen) {
    inflate_state s;
    memset(&s, 0, sizeof s);
    s.in = in;
    s.inlen = inlen;
    int rc = inflate_raw(&s);
    if (rc) { free(s.out.buf); return rc; }
    *out = s.out.buf;
    *outlen = s.out.len;
    return 0;
}

/* gzip: header per RFC 1952 section 2.3, then one deflate stream. Concatenated
 * members are decoded in turn. The 8-byte trailer (CRC, ISIZE) is skipped. */
int blockos_gunzip(const uint8_t *in, size_t inlen, uint8_t **out, size_t *outlen) {
    inflate_state s;
    memset(&s, 0, sizeof s);
    s.in = in;
    s.inlen = inlen;
    while (s.inpos < s.inlen) {
        if (s.inpos + 10 > s.inlen) break;
        const uint8_t *h = s.in + s.inpos;
        if (h[0] != 0x1f || h[1] != 0x8b || h[2] != 8) { free(s.out.buf); return -20; }
        uint8_t flags = h[3];
        if (flags & 0xe0) { free(s.out.buf); return -21; }
        size_t p = s.inpos + 10;
        if (flags & 0x04) { /* FEXTRA */
            if (p + 2 > s.inlen) { free(s.out.buf); return -22; }
            p += 2 + (size_t)(s.in[p] | (s.in[p + 1] << 8));
        }
        if (flags & 0x08) { /* FNAME */
            while (p < s.inlen && s.in[p]) p++;
            p++;
        }
        if (flags & 0x10) { /* FCOMMENT */
            while (p < s.inlen && s.in[p]) p++;
            p++;
        }
        if (flags & 0x02) p += 2; /* FHCRC */
        if (p > s.inlen) { free(s.out.buf); return -22; }
        s.inpos = p;
        s.bitbuf = 0;
        s.bitcnt = 0;
        int rc = inflate_raw(&s);
        if (rc) { free(s.out.buf); return rc; }
        s.inpos += 8; /* CRC32 + ISIZE */
        if (s.inpos > s.inlen) s.inpos = s.inlen;
    }
    if (!s.out.buf) { s.out.buf = (uint8_t *)malloc(1); }
    *out = s.out.buf;
    *outlen = s.out.len;
    return s.out.buf ? 0 : -2;
}

/* ================================================================ bzip2 */

typedef struct {
    const uint8_t *in;
    size_t inlen;
    size_t inpos;
    uint32_t bitbuf;
    int bitcnt;
} bz_reader;

/* MSB-first bit reader (bzip2 bit order). */
static int bz_bits(bz_reader *r, int n) {
    uint32_t v = 0;
    for (int i = 0; i < n; i++) {
        if (r->bitcnt == 0) {
            if (r->inpos >= r->inlen) return -1;
            r->bitbuf = r->in[r->inpos++];
            r->bitcnt = 8;
        }
        r->bitcnt--;
        v = (v << 1) | ((r->bitbuf >> r->bitcnt) & 1);
    }
    return (int)v;
}

#define BZ_MAX_GROUPS 6
#define BZ_MAX_ALPHA 258
#define BZ_MAX_CODELEN 20

typedef struct {
    int minLen, maxLen;
    int32_t limit[BZ_MAX_CODELEN + 2];
    int32_t base[BZ_MAX_CODELEN + 2];
    uint16_t perm[BZ_MAX_ALPHA];
} bz_table;

static int bz_make_table(bz_table *t, const uint8_t *length, int alphaSize) {
    t->minLen = 32;
    t->maxLen = 0;
    for (int i = 0; i < alphaSize; i++) {
        if (length[i] > t->maxLen) t->maxLen = length[i];
        if (length[i] < t->minLen) t->minLen = length[i];
    }
    int pp = 0;
    for (int len = t->minLen; len <= t->maxLen; len++)
        for (int j = 0; j < alphaSize; j++)
            if (length[j] == len) t->perm[pp++] = (uint16_t)j;

    int32_t base[BZ_MAX_CODELEN + 2];
    for (int i = 0; i < BZ_MAX_CODELEN + 2; i++) base[i] = 0;
    for (int i = 0; i < alphaSize; i++) base[length[i] + 1]++;
    for (int i = 1; i < BZ_MAX_CODELEN + 2; i++) base[i] += base[i - 1];

    int32_t vec = 0;
    for (int i = t->minLen; i <= t->maxLen; i++) {
        vec += (base[i + 1] - base[i]);
        t->limit[i] = vec - 1;
        vec <<= 1;
    }
    for (int i = t->minLen + 1; i <= t->maxLen; i++)
        base[i] = ((t->limit[i - 1] + 1) << 1) - base[i];
    for (int i = 0; i < BZ_MAX_CODELEN + 2; i++) t->base[i] = base[i];
    for (int i = 0; i < t->minLen; i++) t->limit[i] = -1;
    return 0;
}

static int bz_decode_sym(bz_reader *r, const bz_table *t) {
    int zn = t->minLen;
    int zvec = bz_bits(r, zn);
    if (zvec < 0) return -1;
    while (zvec > t->limit[zn]) {
        if (++zn > t->maxLen) return -2;
        int b = bz_bits(r, 1);
        if (b < 0) return -1;
        zvec = (zvec << 1) | b;
    }
    int idx = zvec - t->base[zn];
    if (idx < 0 || idx >= BZ_MAX_ALPHA) return -3;
    return t->perm[idx];
}

/* Decodes one block (after its 48-bit magic) and appends the RLE1-decoded
 * bytes to out. */
static int bz_block(bz_reader *r, int blockSizeMax, outbuf *out) {
    (void)bz_bits(r, 16); /* block CRC high half */
    int crc_lo = bz_bits(r, 16); if (crc_lo < 0) return -1;
    int rand = bz_bits(r, 1); if (rand < 0) return -1;
    if (rand) return -30; /* randomised blocks: not produced by modern bzip2 */
    int origPtr = bz_bits(r, 24); if (origPtr < 0) return -1;

    /* symbol map */
    uint8_t seqToUnseq[256];
    int nInUse = 0;
    int used16 = bz_bits(r, 16); if (used16 < 0) return -1;
    for (int i = 0; i < 16; i++) {
        if (used16 & (0x8000 >> i)) {
            int w = bz_bits(r, 16); if (w < 0) return -1;
            for (int j = 0; j < 16; j++)
                if (w & (0x8000 >> j)) seqToUnseq[nInUse++] = (uint8_t)(i * 16 + j);
        }
    }
    if (nInUse == 0) return -31;
    int alphaSize = nInUse + 2;

    int nGroups = bz_bits(r, 3); if (nGroups < 2 || nGroups > BZ_MAX_GROUPS) return -32;
    int nSelectors = bz_bits(r, 15); if (nSelectors < 1) return -33;
    uint8_t selectorMtf[32768];
    for (int i = 0; i < nSelectors; i++) {
        int j = 0;
        for (;;) {
            int b = bz_bits(r, 1); if (b < 0) return -1;
            if (!b) break;
            if (++j >= nGroups) return -34;
        }
        selectorMtf[i] = (uint8_t)j;
    }
    /* undo the MTF on the selectors */
    uint8_t pos[BZ_MAX_GROUPS];
    for (int v = 0; v < nGroups; v++) pos[v] = (uint8_t)v;
    uint8_t *selector = (uint8_t *)malloc((size_t)nSelectors);
    if (!selector) return -2;
    for (int i = 0; i < nSelectors; i++) {
        int v = selectorMtf[i];
        uint8_t tmp = pos[v];
        while (v > 0) { pos[v] = pos[v - 1]; v--; }
        pos[0] = tmp;
        selector[i] = tmp;
    }

    bz_table tables[BZ_MAX_GROUPS];
    for (int t = 0; t < nGroups; t++) {
        uint8_t len[BZ_MAX_ALPHA];
        int curr = bz_bits(r, 5);
        if (curr < 0) { free(selector); return -1; }
        for (int i = 0; i < alphaSize; i++) {
            for (;;) {
                if (curr < 1 || curr > BZ_MAX_CODELEN) { free(selector); return -35; }
                int b = bz_bits(r, 1); if (b < 0) { free(selector); return -1; }
                if (!b) break;
                int b2 = bz_bits(r, 1); if (b2 < 0) { free(selector); return -1; }
                curr += b2 ? -1 : 1;
            }
            len[i] = (uint8_t)curr;
        }
        bz_make_table(&tables[t], len, alphaSize);
    }

    /* MTF + RLE2 decode into tt (byte values), counting byte frequencies */
    uint32_t *tt = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)blockSizeMax);
    if (!tt) { free(selector); return -2; }
    uint32_t unzftab[256];
    for (int i = 0; i < 256; i++) unzftab[i] = 0;
    uint8_t mtf[256];
    for (int i = 0; i < 256; i++) mtf[i] = (uint8_t)i;
    int EOB = nInUse + 1;
    uint32_t nblock = 0;
    int groupNo = -1, groupPos = 0;
    uint32_t runAcc = 0, runN = 0;
    int rc = 0;

    for (;;) {
        if (groupPos == 0) {
            groupNo++;
            if (groupNo >= nSelectors) { rc = -36; break; }
            groupPos = 50;
        }
        groupPos--;
        int sym = bz_decode_sym(r, &tables[selector[groupNo]]);
        if (sym < 0) { rc = -1; break; }

        if (sym == 0 || sym == 1) { /* RUNA / RUNB */
            if (runN == 0) runN = 1, runAcc = 0;
            runAcc += (sym == 0 ? 1u : 2u) * runN;
            runN <<= 1;
            if (runN > (1u << 24)) { rc = -37; break; }
            continue;
        }
        if (runN) { /* flush pending run of the front symbol */
            uint8_t uc = seqToUnseq[mtf[0]];
            if (nblock + runAcc > (uint32_t)blockSizeMax) { rc = -38; break; }
            unzftab[uc] += runAcc;
            while (runAcc--) tt[nblock++] = uc;
            runN = 0;
        }
        if (sym == EOB) break;

        /* move-to-front of index sym-1 */
        int idx = sym - 1;
        if (idx < 0 || idx >= nInUse) { rc = -39; break; }
        uint8_t v = mtf[idx];
        for (int k = idx; k > 0; k--) mtf[k] = mtf[k - 1];
        mtf[0] = v;
        uint8_t uc = seqToUnseq[v];
        if (nblock >= (uint32_t)blockSizeMax) { rc = -38; break; }
        unzftab[uc]++;
        tt[nblock++] = uc;
    }
    free(selector);
    if (rc) { free(tt); return rc; }
    if (origPtr >= (int)nblock && nblock > 0) { free(tt); return -40; }

    /* inverse BWT: link each position into its successor (bzip2 "fast" method) */
    uint32_t cftab[256];
    cftab[0] = 0;
    for (int i = 1; i < 256; i++) cftab[i] = cftab[i - 1] + unzftab[i - 1];
    for (uint32_t i = 0; i < nblock; i++) {
        uint8_t uc = (uint8_t)(tt[i] & 0xff);
        tt[cftab[uc]++] |= (i << 8);
    }

    /* walk the chain and undo RLE1 (4 equal bytes are followed by a count) */
    if (nblock) {
        uint32_t tPos = tt[origPtr] >> 8;
        int last = -1, same = 0;
        for (uint32_t i = 0; i < nblock; i++) {
            tPos = tt[tPos];
            uint8_t ch = (uint8_t)(tPos & 0xff);
            tPos >>= 8;
            if (same == 4) {
                for (uint8_t k = 0; k < ch; k++)
                    if (out_put(out, (uint8_t)last)) { free(tt); return -2; }
                same = 0;
                last = -1;
                continue;
            }
            if (ch == last) same++;
            else { same = 1; last = ch; }
            if (out_put(out, ch)) { free(tt); return -2; }
        }
    }
    free(tt);
    (void)crc_lo;
    return 0;
}

int blockos_bunzip2(const uint8_t *in, size_t inlen, uint8_t **out, size_t *outlen) {
    bz_reader r;
    outbuf o = { NULL, 0, 0 };
    r.in = in;
    r.inlen = inlen;
    r.inpos = 0;
    r.bitbuf = 0;
    r.bitcnt = 0;

    while (r.inpos < r.inlen) {
        /* stream header "BZh1".."BZh9" */
        int b = bz_bits(&r, 8); if (b != 'B') goto fail;
        b = bz_bits(&r, 8); if (b != 'Z') goto fail;
        b = bz_bits(&r, 8); if (b != 'h') goto fail;
        int level = bz_bits(&r, 8); if (level < '1' || level > '9') goto fail;
        int blockSizeMax = (level - '0') * 100000;

        for (;;) {
            uint32_t hi = (uint32_t)bz_bits(&r, 24);
            uint32_t lo = (uint32_t)bz_bits(&r, 24);
            if (hi == 0x314159 && lo == 0x265359) {
                int rc = bz_block(&r, blockSizeMax, &o);
                if (rc) { free(o.buf); return rc; }
            } else if (hi == 0x177245 && lo == 0x385090) {
                (void)bz_bits(&r, 16); (void)bz_bits(&r, 16); /* stream CRC */
                r.bitcnt = 0; /* streams are byte aligned */
                break;
            } else {
                goto fail;
            }
        }
    }
    if (!o.buf) o.buf = (uint8_t *)malloc(1);
    *out = o.buf;
    *outlen = o.len;
    return o.buf ? 0 : -2;
fail:
    free(o.buf);
    return -41;
}
