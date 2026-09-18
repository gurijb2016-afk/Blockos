#include "iconv.h"
#include "errno.h"
#include <string.h>
struct conv { int passthrough; };
iconv_t iconv_open(const char* to, const char* from) {
    if (!to || !from) { errno = EINVAL; return (iconv_t)-1; }
    static struct conv c = { 1 };
    if (strcmp(to,"UTF-8") == 0 && strcmp(from,"UTF-8") == 0) return &c;
    errno = EINVAL; return (iconv_t)-1;
}
size_t iconv(iconv_t h, char** in, size_t* inbytes, char** out, size_t* outbytes) {
    if (!h || !in || !inbytes || !out || !outbytes) { errno = EINVAL; return (size_t)-1; }
    size_t n = *inbytes < *outbytes ? *inbytes : *outbytes;
    memcpy(*out, *in, n); *in += n; *inbytes -= n; *out += n; *outbytes -= n;
    if (*inbytes) { errno = E2BIG; return (size_t)-1; }
    return 0;
}
int iconv_close(iconv_t h) { return h ? 0 : -1; }
