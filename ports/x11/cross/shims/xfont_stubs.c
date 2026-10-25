/* BlockOS shim for libXfont2: the font server client (src/fc) is not built
 * yet, so its registration hook is a no-op. Compressed font input (gzip,
 * bzip2) is provided by blockos_bufio.c + blockos_decomp.c. */
#include <X11/fonts/bufio.h>
#include <stddef.h>

void fs_register_fpe_functions(void) { }

