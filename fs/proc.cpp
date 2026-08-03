#include "vfs.hpp"
#include "../kernel/allocator.hpp"

#include <stdint.h>
#include <stddef.h>

namespace Blockos {
namespace proc {

/*
 * ============================================================
 *  Egyszerű kernel-string segédfüggvények
 * ============================================================
 */

static size_t str_len(const char* str)
{
    if (!str)
        return 0;

    size_t len = 0;

    while (str[len] != '\0')
        ++len;

    return len;
}

static void str_copy(
    char* dest,
    size_t dest_size,
    const char* src)
{
    if (!dest || dest_size == 0)
        return;

    if (!src)
    {
        dest[0] = '\0';
        return;
    }

    size_t i = 0;

    while (i + 1 < dest_size && src[i] != '\0')
    {
        dest[i] = src[i];
        ++i;
    }

    dest[i] = '\0';
}

static void mem_copy(
    uint8_t* dest,
    const uint8_t* src,
    size_t size)
{
    if (!dest || !src)
        return;

    for (size_t i = 0; i < size; ++i)
        dest[i] = src[i];
}


/*
 * ============================================================
 *  uint64 -> string
 * ============================================================
 */

static size_t uint_to_str(
    uint64_t value,
    char* buffer,
    size_t buffer_size)
{
    if (!buffer || buffer_size == 0)
        return 0;

    if (value == 0)
    {
        if (buffer_size < 2)
            return 0;

        buffer[0] = '0';
        buffer[1] = '\0';
        return 1;
    }

    char tmp[32];
    size_t count = 0;

    while (value != 0 && count < sizeof(tmp))
    {
        tmp[count++] =
            static_cast<char>('0' + (value % 10));

        value /= 10;
    }

    if (count + 1 > buffer_size)
        return 0;

    size_t out = 0;

    while (count > 0)
    {
        buffer[out++] = tmp[--count];
    }

    buffer[out] = '\0';

    return out;
}


/*
 * ============================================================
 *  Buffer append
 * ============================================================
 */

static bool append_string(
    char* buffer,
    size_t max_size,
    size_t& position,
    const char* text)
{
    if (!buffer || !text)
        return false;

    size_t len = str_len(text);

    if (position + len >= max_size)
        return false;

    for (size_t i = 0; i < len; ++i)
        buffer[position++] = text[i];

    buffer[position] = '\0';

    return true;
}

static bool append_uint(
    char* buffer,
    size_t max_size,
    size_t& position,
    uint64_t value)
{
    char number[32];

    size_t len =
        uint_to_str(
            value,
            number,
            sizeof(number));

    if (len == 0)
        return false;

    return append_string(
        buffer,
        max_size,
        position,
        number);
}


/*
 * ============================================================
 *  /proc/meminfo
 *
 *  A jelenlegi allocator API csak:
 *
 *      allocator::init()
 *      allocator::alloc()
 *      allocator::reset()
 *
 *  ezért nincs kitalált get_free_memory() /
 *  get_total_memory() hívás.
 * ============================================================
 */

static size_t read_meminfo(
    char* buffer,
    size_t max_size)
{
    if (!buffer || max_size == 0)
        return 0;

    size_t pos = 0;

    /*
     * A tényleges memória-statisztika jelenlegi allocator
     * interfészén keresztül nem kérhető le.
     *
     * Ezért egy korrekt, nem hazudó státuszt adunk.
     */

    append_string(
        buffer,
        max_size,
        pos,
        "MemTotal: unknown kB\n");

    append_string(
        buffer,
        max_size,
        pos,
        "MemFree: unknown kB\n");

    append_string(
        buffer,
        max_size,
        pos,
        "MemAvailable: unknown kB\n");

    append_string(
        buffer,
        max_size,
        pos,
        "BlockOSAllocator: active\n");

    return pos;
}


/*
 * ============================================================
 *  /proc/version
 * ============================================================
 */

static size_t read_version(
    char* buffer,
    size_t max_size)
{
    if (!buffer || max_size == 0)
        return 0;

    const char* version =
        "BlockOS kernel v1.0.0 "
        "(x86_64, UEFI)\n";

    size_t len = str_len(version);

    if (len >= max_size)
        len = max_size - 1;

    mem_copy(
        reinterpret_cast<uint8_t*>(buffer),
        reinterpret_cast<const uint8_t*>(version),
        len);

    buffer[len] = '\0';

    return len;
}


/*
 * ============================================================
 *  /proc/uptime
 *
 *  Jelenleg nincs olyan timer API megadva ebben a modulban,
 *  ezért itt egy biztonságos placeholder érték szerepel.
 * ============================================================
 */

static size_t read_uptime(
    char* buffer,
    size_t max_size)
{
    if (!buffer || max_size == 0)
        return 0;

    size_t pos = 0;

    append_string(
        buffer,
        max_size,
        pos,
        "0.00 0.00\n");

    return pos;
}


/*
 * ============================================================
 *  /proc/filesystems
 * ============================================================
 */

static size_t read_filesystems(
    char* buffer,
    size_t max_size)
{
    if (!buffer || max_size == 0)
        return 0;

    size_t pos = 0;

    append_string(
        buffer,
        max_size,
        pos,
        "nodev\tproc\n");

    append_string(
        buffer,
        max_size,
        pos,
        "nodev\tsysfs\n");

    append_string(
        buffer,
        max_size,
        pos,
        "nodev\tramfs\n");

    append_string(
        buffer,
        max_size,
        pos,
        "ext4\n");

    append_string(
        buffer,
        max_size,
        pos,
        "ext2\n");

    return pos;
}


/*
 * ============================================================
 *  /proc/cmdline
 * ============================================================
 */

static size_t read_cmdline(
    char* buffer,
    size_t max_size)
{
    if (!buffer || max_size == 0)
        return 0;

    const char* cmdline =
        "BOOT_IMAGE=BlockOS";

    size_t len = str_len(cmdline);

    if (len + 2 > max_size)
        len = max_size - 2;

    mem_copy(
        reinterpret_cast<uint8_t*>(buffer),
        reinterpret_cast<const uint8_t*>(cmdline),
        len);

    buffer[len++] = '\n';
    buffer[len] = '\0';

    return len;
}


/*
 * ============================================================
 *  /proc/self
 *
 *  Mivel a jelenlegi scheduler.hpp pontos API-ja nincs ebben
 *  az interfészben definiálva, nem hívunk nem létező
 *  Scheduler::get_task_by_pid() függvényt.
 * ============================================================
 */

static size_t read_self_status(
    char* buffer,
    size_t max_size)
{
    if (!buffer || max_size == 0)
        return 0;

    size_t pos = 0;

    append_string(
        buffer,
        max_size,
        pos,
        "Name:\tBlockOS\n");

    append_string(
        buffer,
        max_size,
        pos,
        "State:\tRunning\n");

    append_string(
        buffer,
        max_size,
        pos,
        "Pid:\t0\n");

    append_string(
        buffer,
        max_size,
        pos,
        "Arch:\tx86_64\n");

    return pos;
}


/*
 * ============================================================
 *  Virtuális proc fájlok
 *
 *  Ezeket a jelenlegi egyszerű vfs API-val lehet felhasználni.
 * ============================================================
 */

struct ProcFile
{
    const char* name;

    size_t (*read)(
        char* buffer,
        size_t max_size);
};

static const ProcFile proc_files[] =
{
    {
        "meminfo",
        read_meminfo
    },

    {
        "version",
        read_version
    },

    {
        "uptime",
        read_uptime
    },

    {
        "filesystems",
        read_filesystems
    },

    {
        "cmdline",
        read_cmdline
    },

    {
        "self/status",
        read_self_status
    }
};


static constexpr size_t PROC_FILE_COUNT =
    sizeof(proc_files) / sizeof(proc_files[0]);


/*
 * ============================================================
 *  proc fájl keresése
 * ============================================================
 */

static const ProcFile* find_file(
    const char* name)
{
    if (!name)
        return nullptr;

    for (size_t i = 0; i < PROC_FILE_COUNT; ++i)
    {
        if (str_len(name) != str_len(proc_files[i].name))
            continue;

        bool equal = true;

        for (size_t j = 0;
             proc_files[i].name[j] != '\0';
             ++j)
        {
            if (name[j] != proc_files[i].name[j])
            {
                equal = false;
                break;
            }
        }

        if (equal)
            return &proc_files[i];
    }

    return nullptr;
}


/*
 * ============================================================
 *  proc_read()
 *
 *  Példa:
 *
 *      proc::read("meminfo", buffer, sizeof(buffer));
 * ============================================================
 */

size_t read(
    const char* name,
    char* buffer,
    size_t max_size)
{
    if (!name || !buffer || max_size == 0)
        return 0;

    const ProcFile* file =
        find_file(name);

    if (!file || !file->read)
        return 0;

    return file->read(
        buffer,
        max_size);
}


/*
 * ============================================================
 *  proc_exists()
 * ============================================================
 */

bool exists(
    const char* name)
{
    return find_file(name) != nullptr;
}


/*
 * ============================================================
 *  proc_count()
 * ============================================================
 */

size_t count()
{
    return PROC_FILE_COUNT;
}


/*
 * ============================================================
 *  proc_name_at()
 * ============================================================
 */

const char* name_at(
    size_t index)
{
    if (index >= PROC_FILE_COUNT)
        return nullptr;

    return proc_files[index].name;
}


/*
 * ============================================================
 *  init
 *
 *  FONTOS:
 *
 *  A jelenlegi fs/vfs.hpp-ban nincs:
 *
 *      VFS::register_filesystem()
 *
 *  ezért itt nem hívunk nem létező API-t.
 *
 *  Később, amikor a valódi VFS mount/register interfész
 *  elkészül, ezt lehet bekötni.
 * ============================================================
 */

void init()
{
    /*
     * A proc alrendszer statikus.
     *
     * Nincs szükség heap-allokációra vagy
     * VFS::FileSystem példányra.
     */
}


/*
 * ============================================================
 *  Debug / teszt
 * ============================================================
 */

bool test()
{
    char buffer[512];

    size_t size =
        read(
            "version",
            buffer,
            sizeof(buffer));

    return size > 0;
}

} // namespace proc
} // namespace Blockos


/*
 * ============================================================
 *  Régi init_proc_fs() kompatibilitási wrapper
 * ============================================================
 *
 * Ha a projekt valamelyik másik része még ezt hívja:
 *
 *      init_proc_fs();
 *
 * akkor továbbra is működjön.
 */

void init_proc_fs()
{
    Blockos::proc::init();
}
