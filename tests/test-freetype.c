#include <stdio.h>
#include <ft2build.h>
#include FT_FREETYPE_H

int main(void) {
    FT_Library lib = NULL;
    FT_Int major = 0, minor = 0, patch = 0;
    if (FT_Init_FreeType(&lib) != 0) return 10;
    FT_Library_Version(lib, &major, &minor, &patch);
    printf("FreeType %d.%d.%d\n", major, minor, patch);
    FT_Done_FreeType(lib);
    return 0;
}
