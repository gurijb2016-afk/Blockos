#include "ramfs.h"

#include <stddef.h>

namespace {

static size_t string_length(const char* s)
{
    if (!s)
        return 0;

    size_t n = 0;
    while (s[n] != '\0')
        ++n;
    return n;
}

static bool string_equal(const char* a, const char* b)
{
    if (!a || !b)
        return false;

    size_t i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i])
            return false;
        ++i;
    }

    return a[i] == b[i];
}

static size_t ramfs_count()
{
    const ramfile* first = __blockos_ramfs_start;
    const ramfile* last = __blockos_ramfs_end;

    if (!first || !last || last < first)
        return 0;

    return static_cast<size_t>(last - first);
}

} // namespace

extern "C" const uint8_t* ramfs_get(const char* name, uint32_t* size_out)
{
    if (size_out)
        *size_out = 0;

    if (!name)
        return nullptr;

    const size_t wanted_length = string_length(name);
    if (wanted_length == 0)
        return nullptr;

    const size_t count = ramfs_count();

    for (size_t i = 0; i < count; ++i) {
        const ramfile& file = __blockos_ramfs_start[i];
        if (!file.name || !file.data)
            continue;

        if (!string_equal(file.name, name))
            continue;

        if (size_out)
            *size_out = file.size;

        return file.data;
    }

    return nullptr;
}
