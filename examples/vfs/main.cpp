#include "fs/vfs.hpp"
#include <stdint.h>

void example_vfs()
{
    const size_t count = vfs::count_files();
    for (size_t i = 0; i < count; ++i) {
        const char* name = vfs::name_at(i);
        if (!name)
            continue;

        uint32_t size = 0;
        const uint8_t* data = vfs::read_file(name, &size);
        (void)data;
        (void)size;
    }

    static const uint8_t text[] = "created by BlockOS VFS example\n";
    vfs::create_file("/blockos/vfs-example.txt", text, sizeof(text) - 1);
}
