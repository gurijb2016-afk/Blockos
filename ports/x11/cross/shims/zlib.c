#include "zlib.h"
gzFile gzopen(const char *path, const char *mode) { return fopen(path, mode); }
int gzgetc(gzFile f) { return fgetc(f); }
int gzclose(gzFile f) { return fclose(f); }
