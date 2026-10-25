/* BlockOS: BufFilePushZIP / BufFilePushBZIP2 for libXfont2. Replaces the
 * gunzip.c / bunzip2.c files that the BlockOS build leaves out (they need the
 * real zlib and libbz2). The underlying stream is read to EOF and decoded in
 * memory by blockos_decomp.c; the BufFile then serves the decoded bytes. */
#include <stdlib.h>
#include <stdint.h>
#include <X11/fonts/bufio.h>
#include "blockos_decomp.h"

typedef struct {
    uint8_t *buf;
    size_t len;
    BufFilePtr inner;
} decoded_buf;

/* BufFileGet() reads f->bufp/f->left directly on its fast path, without
 * calling back into us. So the read position is always taken from f->bufp
 * (the next unread byte), never from a private counter that the fast path
 * would leave stale. */
static size_t decoded_pos(BufFilePtr f, decoded_buf *d) {
    if (!d->buf || !f->bufp) return d->len;
    if (f->bufp < d->buf) return d->len;
    size_t p = (size_t)(f->bufp - d->buf);
    return p > d->len ? d->len : p;
}

static void decoded_set(BufFilePtr f, decoded_buf *d, size_t pos) {
    f->bufp = d->buf + pos;
    f->left = (int)(d->len - pos);
}

static int decoded_input(BufFilePtr f) {
    decoded_buf *d = (decoded_buf *)f->private;
    size_t pos = decoded_pos(f, d);
    if (pos >= d->len) {
        f->left = 0;
        return BUFFILEEOF;
    }
    int c = d->buf[pos];
    decoded_set(f, d, pos + 1);
    return c;
}

static int decoded_skip(BufFilePtr f, int count) {
    decoded_buf *d = (decoded_buf *)f->private;
    if (count < 0) return BUFFILEEOF;
    size_t pos = decoded_pos(f, d);
    size_t left = d->len - pos;
    if ((size_t)count > left) {
        decoded_set(f, d, d->len);
        return BUFFILEEOF;
    }
    decoded_set(f, d, pos + (size_t)count);
    return count;
}

static int decoded_close(BufFilePtr f, int flag) {
    decoded_buf *d = (decoded_buf *)f->private;
    free(d->buf);
    if (d->inner) BufFileClose(d->inner, flag);
    free(d);
    return 1;
}

/* Reads the whole compressed stream from inner. Returns NULL on any error. */
static BufFilePtr push_decoded(BufFilePtr inner, int bzip2) {
    size_t cap = 65536, n = 0;
    uint8_t *in = (uint8_t *)malloc(cap);
    if (!in) return NULL;
    int c;
    while ((c = BufFileGet(inner)) != BUFFILEEOF) {
        if (n == cap) {
            uint8_t *nb = (uint8_t *)realloc(in, cap * 2);
            if (!nb) { free(in); return NULL; }
            in = nb;
            cap *= 2;
        }
        in[n++] = (uint8_t)c;
    }
    uint8_t *out = NULL;
    size_t outlen = 0;
    int rc = bzip2 ? blockos_bunzip2(in, n, &out, &outlen)
                   : blockos_gunzip(in, n, &out, &outlen);
    free(in);
    if (rc) return NULL;

    decoded_buf *d = (decoded_buf *)malloc(sizeof *d);
    if (!d) { free(out); return NULL; }
    d->buf = out;
    d->len = outlen;
    d->inner = inner;

    BufFilePtr f = BufFileCreate((char *)d, decoded_input, 0, decoded_skip, decoded_close);
    if (!f) { free(out); free(d); return NULL; }
    /* Start reading at the decoded buffer, not at BufFileCreate's inline buffer. */
    decoded_set(f, d, 0);
    return f;
}

BufFilePtr BufFilePushZIP(BufFilePtr f) {
    return push_decoded(f, 0);
}

BufFilePtr BufFilePushBZIP2(BufFilePtr f) {
    return push_decoded(f, 1);
}
