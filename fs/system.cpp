#include "fs/vfs.hpp"
#include <cstddef>
#include <cstdint>

namespace Blockos {

/* ============================================================
 * Freestanding string/memory segédfüggvények
 * ============================================================ */

static size_t str_len(const char* s)
{
    if (!s) {
        return 0;
    }

    size_t n = 0;

    while (s[n] != '\0') {
        ++n;
    }

    return n;
}

static int str_cmp(const char* a, const char* b)
{
    if (!a || !b) {
        if (a == b) {
            return 0;
        }

        return a ? 1 : -1;
    }

    size_t i = 0;

    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return (unsigned char)a[i] - (unsigned char)b[i];
        }

        ++i;
    }

    return (unsigned char)a[i] - (unsigned char)b[i];
}

static void mem_copy(
    uint8_t* dest,
    const uint8_t* src,
    size_t size
)
{
    if (!dest || !src) {
        return;
    }

    for (size_t i = 0; i < size; ++i) {
        dest[i] = src[i];
    }
}

static size_t append_text(
    char* buffer,
    size_t max,
    size_t pos,
    const char* text
)
{
    if (!buffer || !text || pos >= max) {
        return pos;
    }

    size_t len = str_len(text);

    size_t available = max - pos;

    if (len > available) {
        len = available;
    }

    mem_copy(
        reinterpret_cast<uint8_t*>(buffer + pos),
        reinterpret_cast<const uint8_t*>(text),
        len
    );

    return pos + len;
}

/* ============================================================
 * System filesystem adatok
 * ============================================================ */

static const char* system_services =
    "--- BlockOS Active Services ---\n"
    "[ACTIVE]  udevd.service\n"
    "[ACTIVE]  knetworkd.service\n"
    "[STANDBY] gui_compositor.service\n"
    "[ACTIVE]  filesystem.service\n"
    "[ACTIVE]  proc.service\n"
    "[ACTIVE]  sysfs.service\n";

static const char* system_pci_devices =
    "--- PCI Bus Topology ---\n"
    "Bus 00 Slot 01.0: VirtIO Network Card "
    "(Vendor: 0x1AF4 Device: 0x1000)\n"
    "Bus 00 Slot 02.0: VirtIO Block Device "
    "(Vendor: 0x1AF4 Device: 0x1001)\n"
    "Bus 00 Slot 03.0: Standard VGA Controller\n";

/* ============================================================
 * /system/services
 * ============================================================ */

static size_t read_services_status(
    char* buffer,
    size_t max
)
{
    if (!buffer || max == 0) {
        return 0;
    }

    size_t pos = 0;

    pos = append_text(
        buffer,
        max,
        pos,
        system_services
    );

    return pos;
}

/* ============================================================
 * /system/pci_devices
 * ============================================================ */

static size_t read_pci_tree(
    char* buffer,
    size_t max
)
{
    if (!buffer || max == 0) {
        return 0;
    }

    size_t pos = 0;

    pos = append_text(
        buffer,
        max,
        pos,
        system_pci_devices
    );

    return pos;
}

/* ============================================================
 * /system/info
 * ============================================================ */

static size_t read_system_info(
    char* buffer,
    size_t max
)
{
    if (!buffer || max == 0) {
        return 0;
    }

    size_t pos = 0;

    pos = append_text(
        buffer,
        max,
        pos,
        "BlockOS System Information\n"
    );

    pos = append_text(
        buffer,
        max,
        pos,
        "---------------------------\n"
    );

    pos = append_text(
        buffer,
        max,
        pos,
        "OS: BlockOS\n"
    );

    pos = append_text(
        buffer,
        max,
        pos,
        "Version: 1.0\n"
    );

    pos = append_text(
        buffer,
        max,
        pos,
        "Architecture: x86_64\n"
    );

    pos = append_text(
        buffer,
        max,
        pos,
        "Kernel: BlockOS Kernel\n"
    );

    pos = append_text(
        buffer,
        max,
        pos,
        "Filesystem API: vfs\n"
    );

    return pos;
}

/* ============================================================
 * /system/mounts
 * ============================================================ */

static size_t read_mounts(
    char* buffer,
    size_t max
)
{
    if (!buffer || max == 0) {
        return 0;
    }

    size_t pos = 0;

    pos = append_text(
        buffer,
        max,
        pos,
        "--- BlockOS Mounted Filesystems ---\n"
    );

    pos = append_text(
        buffer,
        max,
        pos,
        "root     /        ext4\n"
    );

    pos = append_text(
        buffer,
        max,
        pos,
        "proc     /proc    proc\n"
    );

    pos = append_text(
        buffer,
        max,
        pos,
        "sysfs    /system  sysfs\n"
    );

    return pos;
}

/* ============================================================
 * Virtuális system fájlok
 *
 * Mivel a jelenlegi VFS API:
 *
 *   vfs::read_file()
 *   vfs::write_file()
 *   vfs::create_file()
 *
 * ezért a /system fájlokat az egyszerű VFS-be regisztráljuk.
 * ============================================================ */

static bool create_system_file(
    const char* name,
    const char* data
)
{
    if (!name || !data) {
        return false;
    }

    uint32_t size =
        static_cast<uint32_t>(str_len(data));

    return vfs::create_file(
        name,
        reinterpret_cast<const uint8_t*>(data),
        size
    );
}

/* ============================================================
 * System filesystem inicializálása
 * ============================================================ */

bool init_system_fs()
{
    bool ok = true;

    /*
     * A jelenlegi VFS nem rendelkezik külön mountolt
     * FileSystem osztállyal, ezért a /system fájlokat
     * közvetlenül a VFS fájltárolójába tesszük.
     */

    if (!create_system_file(
            "/system/services",
            system_services)) {
        ok = false;
    }

    if (!create_system_file(
            "/system/pci_devices",
            system_pci_devices)) {
        ok = false;
    }

    const char* info =
        "BlockOS System Information\n"
        "---------------------------\n"
        "OS: BlockOS\n"
        "Version: 1.0\n"
        "Architecture: x86_64\n"
        "Kernel: BlockOS Kernel\n"
        "Filesystem API: vfs\n";

    if (!create_system_file(
            "/system/info",
            info)) {
        ok = false;
    }

    const char* mounts =
        "--- BlockOS Mounted Filesystems ---\n"
        "root     /        ext4\n"
        "proc     /proc    proc\n"
        "sysfs    /system  sysfs\n";

    if (!create_system_file(
            "/system/mounts",
            mounts)) {
        ok = false;
    }

    return ok;
}

/* ============================================================
 * System service parancsok
 * ============================================================ */

bool system_reload_services()
{
    static const uint8_t command[] = {
        'r', 'e', 'l', 'o', 'a', 'd'
    };

    return vfs::write_file(
        "/system/services",
        command,
        sizeof(command)
    );
}

/* ============================================================
 * System fájl olvasása
 * ============================================================ */

const uint8_t* system_read_file(
    const char* name,
    uint32_t* size
)
{
    if (!name || !size) {
        return nullptr;
    }

    return vfs::read_file(name, size);
}

/* ============================================================
 * System fájl létrehozása
 * ============================================================ */

bool system_create_file(
    const char* name,
    const uint8_t* data,
    uint32_t size
)
{
    if (!name) {
        return false;
    }

    return vfs::create_file(
        name,
        data,
        size
    );
}

/* ============================================================
 * System fájl írása
 * ============================================================ */

bool system_write_file(
    const char* name,
    const uint8_t* data,
    uint32_t size
)
{
    if (!name) {
        return false;
    }

    return vfs::write_file(
        name,
        data,
        size
    );
}

} // namespace Blockos
