#include <ft2build.h>
#include FT_FREETYPE_H
#include <string.h>
int bcmp(const void *a, const void *b, size_t n) { return memcmp(a, b, n); }
/* Compressed-stream support is not built (no zlib on BlockOS yet): report unsupported. */
FT_Error FT_Stream_OpenGzip(FT_Stream stream, FT_Stream source) { (void)stream; (void)source; return FT_Err_Unimplemented_Feature; }
FT_Error FT_Stream_OpenLZW(FT_Stream stream, FT_Stream source) { (void)stream; (void)source; return FT_Err_Unimplemented_Feature; }
FT_Error FT_Gzip_Uncompress(FT_Memory memory, FT_Byte *output, FT_ULong *out_len, const FT_Byte *input, FT_ULong in_len) { (void)memory; (void)output; (void)out_len; (void)input; (void)in_len; return FT_Err_Unimplemented_Feature; }
