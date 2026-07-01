#include "ramfs.h"

// Put file data here. Each entry is a struct ramfile {name, data ptr, size}.
static const uint8_t readme_txt[] =
"Blockos embedded readme\nThis is a simple in-memory ramfs file.\nYou can drag the window title bar with the left mouse button.\n";

const struct ramfile __ramfs_start[] __attribute__((section(".ramfs"))) = {
    { "readme.txt", readme_txt, (uint32_t)sizeof(readme_txt)-1 }
};
const struct ramfile __ramfs_end[] __attribute__((section(".ramfs"))) = { };
