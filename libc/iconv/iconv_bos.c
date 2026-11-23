/* Real iconv for BlockOS: UTF-8, UTF-16 (LE/BE, UCS-2), UTF-32LE, ISO-8859-1, US-ASCII. */
#include <iconv.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#ifndef EILSEQ
#define EILSEQ 84
#endif
#ifndef E2BIG
#define E2BIG 7
#endif

enum { E_UTF8, E_U16LE, E_U16BE, E_U32LE, E_LATIN1, E_ASCII, E_BAD };
struct bos_iconv { int from, to; };

static int enc_of(const char *n) {
    char buf[32]; size_t i = 0;
    for (; n[i] && n[i] != '/' && i < sizeof(buf) - 1; i++) buf[i] = n[i];
    buf[i] = 0;
    if (!strcasecmp(buf, "UTF-8") || !strcasecmp(buf, "UTF8")) return E_UTF8;
    if (!strcasecmp(buf, "UTF-16") || !strcasecmp(buf, "UTF-16LE") || !strcasecmp(buf, "UCS-2") || !strcasecmp(buf, "UCS-2LE")) return E_U16LE;
    if (!strcasecmp(buf, "UTF-16BE") || !strcasecmp(buf, "UCS-2BE")) return E_U16BE;
    if (!strcasecmp(buf, "UTF-32") || !strcasecmp(buf, "UTF-32LE") || !strcasecmp(buf, "UCS-4")) return E_U32LE;
    if (!strcasecmp(buf, "ISO-8859-1") || !strcasecmp(buf, "ISO8859-1") || !strcasecmp(buf, "LATIN1")) return E_LATIN1;
    if (!strcasecmp(buf, "US-ASCII") || !strcasecmp(buf, "ASCII") || !strcasecmp(buf, "ANSI_X3.4-1968")) return E_ASCII;
    return E_BAD;
}

/* returns bytes consumed, -1 invalid, -2 incomplete */
static int decode(int e, const unsigned char *s, size_t n, uint32_t *cp) {
    if (!n) return -2;
    switch (e) {
    case E_UTF8: {
        unsigned c = s[0]; int len; uint32_t v;
        if (c < 0x80) { *cp = c; return 1; }
        if ((c & 0xE0) == 0xC0) { len = 2; v = c & 0x1F; }
        else if ((c & 0xF0) == 0xE0) { len = 3; v = c & 0x0F; }
        else if ((c & 0xF8) == 0xF0) { len = 4; v = c & 0x07; }
        else return -1;
        if ((size_t)len > n) return -2;
        for (int i = 1; i < len; i++) {
            if ((s[i] & 0xC0) != 0x80) return -1;
            v = (v << 6) | (s[i] & 0x3F);
        }
        *cp = v; return len;
    }
    case E_U16LE: case E_U16BE: {
        if (n < 2) return -2;
        uint32_t u = e == E_U16LE ? (s[0] | (s[1] << 8)) : ((s[0] << 8) | s[1]);
        if (u >= 0xD800 && u <= 0xDBFF) {
            if (n < 4) return -2;
            uint32_t l = e == E_U16LE ? (s[2] | (s[3] << 8)) : ((s[2] << 8) | s[3]);
            if (l < 0xDC00 || l > 0xDFFF) return -1;
            *cp = 0x10000 + ((u - 0xD800) << 10) + (l - 0xDC00); return 4;
        }
        if (u >= 0xDC00 && u <= 0xDFFF) return -1;
        *cp = u; return 2;
    }
    case E_U32LE:
        if (n < 4) return -2;
        *cp = s[0] | (s[1] << 8) | (s[2] << 16) | ((uint32_t)s[3] << 24); return 4;
    case E_LATIN1: *cp = s[0]; return 1;
    case E_ASCII: if (s[0] >= 0x80) return -1; *cp = s[0]; return 1;
    }
    return -1;
}

/* returns bytes written into d (max 4), -1 if not representable */
static int encode(int e, uint32_t cp, unsigned char *d) {
    switch (e) {
    case E_UTF8:
        if (cp < 0x80) { d[0] = cp; return 1; }
        if (cp < 0x800) { d[0] = 0xC0 | (cp >> 6); d[1] = 0x80 | (cp & 0x3F); return 2; }
        if (cp < 0x10000) { d[0] = 0xE0 | (cp >> 12); d[1] = 0x80 | ((cp >> 6) & 0x3F); d[2] = 0x80 | (cp & 0x3F); return 3; }
        if (cp <= 0x10FFFF) { d[0] = 0xF0 | (cp >> 18); d[1] = 0x80 | ((cp >> 12) & 0x3F); d[2] = 0x80 | ((cp >> 6) & 0x3F); d[3] = 0x80 | (cp & 0x3F); return 4; }
        return -1;
    case E_U16LE: case E_U16BE: {
        unsigned char *p = d; int w = 0;
        if (cp >= 0x10000) {
            if (cp > 0x10FFFF) return -1;
            uint32_t v = cp - 0x10000;
            uint32_t hi = 0xD800 + (v >> 10), lo = 0xDC00 + (v & 0x3FF);
            if (e == E_U16LE) { p[0] = hi; p[1] = hi >> 8; p[2] = lo; p[3] = lo >> 8; }
            else { p[0] = hi >> 8; p[1] = hi; p[2] = lo >> 8; p[3] = lo; }
            return 4;
        }
        if (cp >= 0xD800 && cp <= 0xDFFF) return -1;
        if (e == E_U16LE) { p[0] = cp; p[1] = cp >> 8; } else { p[0] = cp >> 8; p[1] = cp; }
        (void)w; return 2;
    }
    case E_U32LE:
        d[0] = cp; d[1] = cp >> 8; d[2] = cp >> 16; d[3] = cp >> 24; return 4;
    case E_LATIN1: if (cp > 0xFF) return -1; d[0] = cp; return 1;
    case E_ASCII: if (cp > 0x7F) return -1; d[0] = cp; return 1;
    }
    return -1;
}

iconv_t iconv_open(const char *to, const char *from) {
    if (!to || !from) { errno = EINVAL; return (iconv_t)-1; }
    int f = enc_of(from), t = enc_of(to);
    if (f == E_BAD || t == E_BAD) { errno = EINVAL; return (iconv_t)-1; }
    struct bos_iconv *h = malloc(sizeof(*h));
    if (!h) { errno = ENOMEM; return (iconv_t)-1; }
    h->from = f; h->to = t;
    return (iconv_t)h;
}

size_t iconv(iconv_t hh, char **in, size_t *inleft, char **out, size_t *outleft) {
    struct bos_iconv *h = (struct bos_iconv *)hh;
    if (!in || !*in) return 0;
    while (*inleft) {
        uint32_t cp = 0; unsigned char tmp[4];
        int n = decode(h->from, (const unsigned char *)*in, *inleft, &cp);
        if (n == -2) { errno = EINVAL; return (size_t)-1; }
        if (n < 0) { errno = EILSEQ; return (size_t)-1; }
        int m = encode(h->to, cp, tmp);
        if (m < 0) { errno = EILSEQ; return (size_t)-1; }
        if (!out || !*out) return (size_t)-1;
        if (*outleft < (size_t)m) { errno = E2BIG; return (size_t)-1; }
        memcpy(*out, tmp, m);
        *out += m; *outleft -= m;
        *in += n; *inleft -= n;
    }
    return 0;
}

int iconv_close(iconv_t hh) { free(hh); return 0; }
