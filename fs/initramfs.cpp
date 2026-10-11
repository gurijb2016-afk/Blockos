#include "fs/initramfs.hpp"
#include "fs/vfs.hpp"

#include <stddef.h>
#include <stdint.h>

extern "C" {
extern const uint8_t _binary_build_initramfs_cpio_start[];
extern const uint8_t _binary_build_initramfs_cpio_end[];
}

namespace {

constexpr size_t CPIO_HEADER_SIZE = 110;
constexpr size_t CPIO_FIELD_MODE = 14;
constexpr size_t CPIO_FIELD_FILESIZE = 54;
constexpr size_t CPIO_FIELD_NAMESIZE = 94;
constexpr size_t MAX_PATH = 256;
constexpr uint32_t S_IFMT_VALUE = 0170000u;
constexpr uint32_t S_IFREG_VALUE = 0100000u;
constexpr uint32_t S_IFDIR_VALUE = 0040000u;

static bool hex_field(const uint8_t* p, uint32_t* out)
{
    if (!p || !out)
        return false;

    uint32_t value = 0;
    for (size_t i = 0; i < 8; ++i) {
        const uint8_t c = p[i];
        uint32_t digit;
        if (c >= '0' && c <= '9')
            digit = c - '0';
        else if (c >= 'a' && c <= 'f')
            digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
            digit = c - 'A' + 10;
        else
            return false;
        value = (value << 4) | digit;
    }
    *out = value;
    return true;
}

static size_t align4(size_t value)
{
    return (value + 3u) & ~static_cast<size_t>(3u);
}

// CPIO stores relative names. Convert them to a safe, absolute VFS path.
// Reject absolute paths and '..' components to prevent archive path traversal.
static bool make_path(const uint8_t* name, size_t length, char* output, size_t capacity)
{
    if (!name || !output || capacity < 2 || length == 0)
        return false;

    size_t start = 0;
    while (start + 1 < length && name[start] == '.' && name[start + 1] == '/')
        start += 2;
    if (start == length || (length - start == 1 && name[start] == '.')) {
        output[0] = '/';
        output[1] = '\0';
        return true;
    }
    if (name[start] == '/')
        return false;

    size_t out = 0;
    output[out++] = '/';
    size_t i = start;
    while (i < length) {
        while (i < length && name[i] == '/')
            ++i;
        if (i >= length)
            break;
        const size_t component_start = i;
        while (i < length && name[i] != '/') {
            if (name[i] == '\0')
                return false;
            ++i;
        }
        const size_t component_length = i - component_start;
        if (component_length == 0)
            continue;
        if (component_length == 1 && name[component_start] == '.')
            continue;
        if (component_length == 2 && name[component_start] == '.' && name[component_start + 1] == '.')
            return false;

        if (out > 1) {
            if (out + 1 >= capacity)
                return false;
            output[out++] = '/';
        }
        if (component_length >= capacity - out)
            return false;
        for (size_t j = 0; j < component_length; ++j)
            output[out++] = static_cast<char>(name[component_start + j]);
    }
    output[out] = '\0';
    return true;
}

static bool ensure_directory(const char* path)
{
    if (!path || path[0] != '/')
        return false;
    if (path[1] == '\0')
        return true;

    char parent[MAX_PATH];
    size_t length = 0;
    while (path[length] && length + 1 < sizeof(parent)) {
        parent[length] = path[length];
        ++length;
    }
    if (path[length] != '\0')
        return false;
    parent[length] = '\0';

    for (size_t i = 1; i <= length; ++i) {
        if (parent[i] != '/' && parent[i] != '\0')
            continue;
        const char saved = parent[i];
        parent[i] = '\0';
        if (parent[0] != '\0' && !vfs::exists(parent)) {
            if (!vfs::create_directory(parent)) {
                parent[i] = saved;
                return false;
            }
        } else if (parent[0] != '\0' && !vfs::is_directory(parent)) {
            parent[i] = saved;
            return false;
        }
        parent[i] = saved;
    }
    return true;
}

static bool install_entry(const char* path, uint32_t mode,
                          const uint8_t* data, uint32_t size)
{
    const uint32_t type = mode & S_IFMT_VALUE;
    if (path[0] == '/' && path[1] == '\0')
        return type == S_IFDIR_VALUE;

    // Make parent directories before installing either directories or files.
    char parent[MAX_PATH];
    size_t length = 0;
    while (path[length] && length + 1 < sizeof(parent)) {
        parent[length] = path[length];
        ++length;
    }
    if (path[length] != '\0')
        return false;
    parent[length] = '\0';
    for (size_t i = length; i > 1; --i) {
        if (parent[i - 1] == '/') {
            parent[i - 1] = '\0';
            if (!ensure_directory(parent))
                return false;
            break;
        }
    }

    if (type == S_IFDIR_VALUE) {
        if (vfs::exists(path))
            return vfs::is_directory(path);
        return vfs::create_directory(path);
    }
    if (type != S_IFREG_VALUE)
        return false; // Devices, FIFOs, sockets and symlink nodes are unsupported.
    if (vfs::exists(path) || (size != 0 && !data))
        return false;
    return vfs::create_file(path, data, size);
}

static bool valid_shell()
{
    uint32_t size = 0;
    const uint8_t* shell = vfs::read_file("/bin/sh", &size);
    return shell && size >= 5 && shell[0] == 0x7f &&
           shell[1] == 'E' && shell[2] == 'L' && shell[3] == 'F' &&
           shell[4] == 2;
}

} // namespace

namespace blockos::initramfs {

bool mount()
{
    const uintptr_t begin = reinterpret_cast<uintptr_t>(_binary_build_initramfs_cpio_start);
    const uintptr_t end = reinterpret_cast<uintptr_t>(_binary_build_initramfs_cpio_end);
    if (begin == 0 || end <= begin)
        return false;

    const uint8_t* const archive = reinterpret_cast<const uint8_t*>(begin);
    const size_t archive_size = static_cast<size_t>(end - begin);
    size_t offset = 0;
    bool found_trailer = false;

    vfs::initialize_devices();

    while (offset < archive_size) {
        if (archive_size - offset < CPIO_HEADER_SIZE)
            return false;
        const uint8_t* header = archive + offset;
        if (header[0] != '0' || header[1] != '7' || header[2] != '0' ||
            header[3] != '7' || header[4] != '0' || header[5] != '1')
            return false; // Only portable newc (070701) is accepted.

        uint32_t mode = 0, file_size = 0, name_size = 0;
        if (!hex_field(header + CPIO_FIELD_MODE, &mode) ||
            !hex_field(header + CPIO_FIELD_FILESIZE, &file_size) ||
            !hex_field(header + CPIO_FIELD_NAMESIZE, &name_size))
            return false;
        if (name_size < 2 || name_size > archive_size - offset - CPIO_HEADER_SIZE)
            return false;

        const size_t name_offset = offset + CPIO_HEADER_SIZE;
        const uint8_t* name = archive + name_offset;
        if (name[name_size - 1] != '\0')
            return false;
        for (size_t i = 0; i + 1 < name_size; ++i) {
            if (name[i] == '\0')
                return false;
        }

        const size_t data_offset = align4(name_offset + name_size);
        if (data_offset < name_offset || data_offset > archive_size ||
            file_size > archive_size - data_offset)
            return false;
        const uint8_t* data = archive + data_offset;
        const size_t next_offset = align4(data_offset + static_cast<size_t>(file_size));
        if (next_offset < data_offset || next_offset > archive_size)
            return false;

        if (name_size == sizeof("TRAILER!!!") &&
            name[0] == 'T' && name[1] == 'R' && name[2] == 'A' &&
            name[3] == 'I' && name[4] == 'L' && name[5] == 'E' &&
            name[6] == 'R' && name[7] == '!' && name[8] == '!' && name[9] == '!') {
            found_trailer = true;
            break;
        }

        char path[MAX_PATH];
        if (!make_path(name, name_size - 1, path, sizeof(path)))
            return false;
        if (!install_entry(path, mode, data, file_size))
            return false;

        offset = next_offset;
    }

    return found_trailer && valid_shell();
}

} // namespace blockos::initramfs
