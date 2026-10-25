/* Minimal zlib shim for BlockOS X libs: plain (uncompressed) files only.
 * Compressed .gz inputs are NOT decompressed yet; install font encodings uncompressed. */
#ifndef BLOCKOS_ZLIB_SHIM_H
#define BLOCKOS_ZLIB_SHIM_H
#include <stdio.h>
typedef FILE *gzFile;
gzFile gzopen(const char *path, const char *mode);
int gzgetc(gzFile f);
int gzclose(gzFile f);
#endif
