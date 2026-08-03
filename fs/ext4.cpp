#include "ext4.hpp"
#include "../drivers/virtio_blk.hpp"

#include <stdint.h>
#include <stddef.h>

namespace
{
    static uint64_t div_round_up_u64(uint64_t value, uint64_t divisor)
    {
        if (divisor == 0)
            return 0;

        return (value + divisor - 1) / divisor;
    }

    static size_t string_length(const char* str)
    {
        if (!str)
            return 0;

        size_t len = 0;

        while (str[len] != '\0')
            ++len;

        return len;
    }

    static bool string_equals(const char* a, const char* b)
    {
        if (!a || !b)
            return false;

        size_t i = 0;

        while (a[i] != '\0' || b[i] != '\0')
        {
            if (a[i] != b[i])
                return false;

            ++i;
        }

        return true;
    }

    static bool string_starts_with(
        const char* str,
        const char* prefix)
    {
        if (!str || !prefix)
            return false;

        size_t i = 0;

        while (prefix[i] != '\0')
        {
            if (str[i] == '\0')
                return false;

            if (str[i] != prefix[i])
                return false;

            ++i;
        }

        return true;
    }

    static uint16_t read_u16(const uint8_t* p)
    {
        return static_cast<uint16_t>(
            static_cast<uint16_t>(p[0]) |
            (static_cast<uint16_t>(p[1]) << 8)
        );
    }

    static uint32_t read_u32(const uint8_t* p)
    {
        return
            static_cast<uint32_t>(p[0]) |
            (static_cast<uint32_t>(p[1]) << 8) |
            (static_cast<uint32_t>(p[2]) << 16) |
            (static_cast<uint32_t>(p[3]) << 24);
    }

    static uint64_t read_u64(const uint8_t* p)
    {
        return
            static_cast<uint64_t>(read_u32(p)) |
            (static_cast<uint64_t>(read_u32(p + 4)) << 32);
    }

    static uint32_t min_u32(
        uint32_t a,
        uint32_t b)
    {
        return (a < b) ? a : b;
    }

    static bool path_component_equal(
        const char* a,
        size_t a_len,
        const char* b,
        size_t b_len)
    {
        if (a_len != b_len)
            return false;

        for (size_t i = 0; i < a_len; ++i)
        {
            if (a[i] != b[i])
                return false;
        }

        return true;
    }
}


/*
 * FONTOS:
 *
 * Az Ext4Reader konstruktora az ext4.hpp-ben már definiálva van.
 *
 * Ezért itt NINCS:
 *
 * Ext4Reader::Ext4Reader()
 *
 * Ez megszünteti a korábbi:
 *
 * redefinition of ‘Ext4Reader::Ext4Reader()’
 *
 * hibát.
 */


/* ============================================================
 * Fizikai EXT4 blokk olvasása
 * ============================================================ */

bool Ext4Reader::read_disk_block(
    uint64_t block_num,
    uint8_t* buffer)
{
    if (!buffer)
        return false;

    if (block_size < 512)
        return false;

    if ((block_size % 512) != 0)
        return false;

    uint32_t sectors_per_block =
        block_size / 512;

    uint64_t first_sector =
        block_num * sectors_per_block;

    for (uint32_t i = 0; i < sectors_per_block; ++i)
    {
        if (!virtio_blk::read_sector(
                first_sector + i,
                buffer + (i * 512)))
        {
            return false;
        }
    }

    return true;
}


/* ============================================================
 * EXT4 superblock olvasása
 * ============================================================ */

bool Ext4Reader::mount()
{
    /*
     * Az EXT4 superblock a lemez 1024. bájtján kezdődik.
     *
     * Ezért először az első 4096 bájtot olvassuk.
     */

    alignas(4096) uint8_t buffer[4096];

    /*
     * A VirtIO blokkdrivernek inicializálva kell lennie.
     *
     * Ha már inicializálva van, az init() true értéket adhat.
     */
    virtio_blk::init();

    /*
     * Először 4 KiB-os olvasást próbálunk.
     */
    block_size = 4096;

    if (!read_disk_block(0, buffer))
        return false;

    /*
     * A superblock a 1024. bájtnál van.
     */
    Ext4Superblock* sb =
        reinterpret_cast<Ext4Superblock*>(buffer + 1024);

    /*
     * EXT2/EXT3/EXT4 magic:
     * 0xEF53
     */
    if (sb->magic != 0xEF53)
    {
        /*
         * Ha a fenti nem sikerült, próbáljuk meg
         * közvetlenül a 1024. bájtos helyet a következő
         * fizikai blokkbeolvasással.
         */
        block_size = 1024;

        if (!read_disk_block(1, buffer))
            return false;

        sb =
            reinterpret_cast<Ext4Superblock*>(buffer);

        if (sb->magic != 0xEF53)
            return false;
    }

    /*
     * A tényleges EXT4 blokkméret:
     *
     * 1024 << log_block_size
     */
    uint32_t detected_block_size =
        1024U << sb->log_block_size;

    if (detected_block_size < 1024)
        return false;

    if (detected_block_size > 4096)
        return false;

    if ((detected_block_size % 512) != 0)
        return false;

    block_size = detected_block_size;

    inodes_per_group =
        sb->inodes_per_group;

    if (inodes_per_group == 0)
        return false;

    /*
     * Az inode méretet az EXT4 superblock
     * s_inode_size mezője tartalmazza.
     *
     * A jelenlegi Ext4Superblock struktúrában ez
     * nincs benne, ezért a rev_level alapján
     * használjuk a szabványos alapértéket.
     */
    if (sb->rev_level == 0)
        inode_size = 128;
    else
        inode_size = 256;

    /*
     * Block Group Descriptor Table:
     *
     * 1 KiB blokk esetén:
     *   block 2
     *
     * 2/4 KiB esetén:
     *   block 1
     */
    if (block_size == 1024)
        bg_desc_table_block = 2;
    else
        bg_desc_table_block = 1;

    return true;
}


/* ============================================================
 * Inode beolvasása
 * ============================================================ */

bool Ext4Reader::get_inode(
    uint32_t inode_num,
    Ext4Inode& target_inode)
{
    if (inode_num == 0)
        return false;

    if (inodes_per_group == 0)
        return false;

    if (inode_size == 0)
        return false;

    /*
     * EXT4 inode numbering 1-től kezdődik.
     */
    uint32_t group =
        (inode_num - 1) / inodes_per_group;

    uint32_t index =
        (inode_num - 1) % inodes_per_group;

    /*
     * Egy blokkcsoport-leíró 32 bájtos
     * minimális formában.
     */
    const uint32_t descriptor_size = 32;

    uint64_t descriptor_offset =
        static_cast<uint64_t>(group) *
        descriptor_size;

    uint64_t descriptor_block =
        bg_desc_table_block +
        (descriptor_offset / block_size);

    uint32_t descriptor_inside =
        static_cast<uint32_t>(
            descriptor_offset % block_size
        );

    alignas(4096) uint8_t gd_buffer[4096];

    if (block_size > sizeof(gd_buffer))
        return false;

    if (!read_disk_block(
            descriptor_block,
            gd_buffer))
    {
        return false;
    }

    if (descriptor_inside + descriptor_size >
        block_size)
    {
        return false;
    }

    /*
     * Az inode_table_lo a group descriptor
     * 8. bájtján kezdődik.
     */
    const uint8_t* gd =
        gd_buffer + descriptor_inside;

    uint32_t inode_table_lo =
        read_u32(gd + 8);

    if (inode_table_lo == 0)
        return false;

    uint64_t inode_byte_offset =
        static_cast<uint64_t>(index) *
        inode_size;

    uint64_t inode_block =
        static_cast<uint64_t>(inode_table_lo) +
        (inode_byte_offset / block_size);

    uint32_t inode_offset =
        static_cast<uint32_t>(
            inode_byte_offset % block_size
        );

    alignas(4096) uint8_t inode_buffer[4096];

    if (block_size > sizeof(inode_buffer))
        return false;

    if (inode_offset + sizeof(Ext4Inode) >
        block_size)
    {
        /*
         * A jelenlegi egyszerű loader nem kezel
         * blokkhatáron átnyúló inode-ot.
         */
        return false;
    }

    if (!read_disk_block(
            inode_block,
            inode_buffer))
    {
        return false;
    }

    /*
     * A célstruktúrát először nullázzuk.
     */
    uint8_t* destination =
        reinterpret_cast<uint8_t*>(&target_inode);

    for (size_t i = 0;
         i < sizeof(Ext4Inode);
         ++i)
    {
        destination[i] = 0;
    }

    /*
     * Csak a struktúra méretéig másolunk.
     */
    const uint8_t* source =
        inode_buffer + inode_offset;

    for (size_t i = 0;
         i < sizeof(Ext4Inode);
         ++i)
    {
        destination[i] = source[i];
    }

    return true;
}


/* ============================================================
 * EXTENT TREE feldolgozása
 * ============================================================ */

void Ext4Reader::parse_extent(
    Ext4ExtentHeader* header,
    uint8_t* dest,
    size_t& bytes_read,
    size_t max_size)
{
    if (!header)
        return;

    if (!dest)
        return;

    if (bytes_read >= max_size)
        return;

    /*
     * EXT4 extent magic.
     */
    if (header->magic != 0xF30A)
        return;

    /*
     * Üres extent tree.
     */
    if (header->entries == 0)
        return;

    /*
     * Leaf node.
     */
    if (header->depth == 0)
    {
        Ext4Extent* extents =
            reinterpret_cast<Ext4Extent*>(
                reinterpret_cast<uint8_t*>(header) +
                sizeof(Ext4ExtentHeader)
            );

        alignas(4096) uint8_t data_buffer[4096];

        if (block_size > sizeof(data_buffer))
            return;

        for (uint16_t i = 0;
             i < header->entries;
             ++i)
        {
            /*
             * A fizikai blokk felső 16 bitje
             * és alsó 32 bitje.
             */
            uint64_t physical_block =
                (static_cast<uint64_t>(
                    extents[i].start_hi) << 32) |
                static_cast<uint64_t>(
                    extents[i].start_lo);

            /*
             * Az EXT4 extent length 16 bites.
             *
             * A felső flag bitet nem szabad
             * normál blokkszámként kezelni.
             */
            uint32_t count =
                static_cast<uint32_t>(
                    extents[i].len & 0x7FFF
                );

            for (uint32_t b = 0;
                 b < count;
                 ++b)
            {
                if (bytes_read >= max_size)
                    return;

                if (!read_disk_block(
                        physical_block + b,
                        data_buffer))
                {
                    return;
                }

                size_t remaining =
                    max_size - bytes_read;

                size_t chunk =
                    (remaining < block_size)
                        ? remaining
                        : block_size;

                for (size_t p = 0;
                     p < chunk;
                     ++p)
                {
                    dest[bytes_read + p] =
                        data_buffer[p];
                }

                bytes_read += chunk;
            }
        }

        return;
    }

    /*
     * Internal extent node.
     *
     * A teljes extent tree támogatása későbbi bővítés.
     * A kis fájlok általában leaf extentben vannak.
     */
}


/* ============================================================
 * Fájl betöltése inode alapján
 * ============================================================ */

bool Ext4Reader::load_file_by_inode(
    uint32_t inode_num,
    uint8_t* dest,
    size_t max_size)
{
    if (inode_num == 0)
        return false;

    if (!dest)
        return false;

    if (max_size == 0)
        return false;

    Ext4Inode inode;

    if (!get_inode(
            inode_num,
            inode))
    {
        return false;
    }

    /*
     * Ellenőrizzük, hogy normál fájl-e.
     *
     * POSIX mód felső 4 bitje:
     *
     * 0x4000 = directory
     * 0x8000 = regular file
     */
    uint16_t file_type =
        inode.mode & 0xF000;

    if (file_type == 0x4000)
        return false;

    if (file_type != 0x8000)
    {
        /*
         * EXT4 inode flag alapján is lehet fájl,
         * ezért itt nem tiltjuk le teljesen.
         */
    }

    /*
     * Fájlméret.
     */
    uint64_t file_size =
        static_cast<uint64_t>(
            inode.size_lo);

    if (inode.size_high != 0)
    {
        file_size |=
            static_cast<uint64_t>(
                inode.size_high) << 32;
    }

    if (file_size == 0)
        return false;

    size_t wanted =
        (file_size < max_size)
            ? static_cast<size_t>(file_size)
            : max_size;

    size_t total_read = 0;

    /*
     * Az inode block mezője 60 bájt.
     * EXT4 esetén ez lehet extent root.
     */
    Ext4ExtentHeader* header =
        reinterpret_cast<Ext4ExtentHeader*>(
            inode.block
        );

    if (header->magic == 0xF30A)
    {
        parse_extent(
            header,
            dest,
            total_read,
            wanted);

        return total_read > 0;
    }

    /*
     * Ha nincs extent tree, a klasszikus
     * direct block támogatás következik.
     *
     * Az inode.block első 12 uint32_t-je
     * direkt blokk.
     */
    const uint32_t* direct_blocks =
        reinterpret_cast<const uint32_t*>(
            inode.block
        );

    alignas(4096) uint8_t data_buffer[4096];

    if (block_size > sizeof(data_buffer))
        return false;

    for (uint32_t i = 0;
         i < 12;
         ++i)
    {
        if (total_read >= wanted)
            break;

        uint32_t physical_block =
            direct_blocks[i];

        if (physical_block == 0)
            continue;

        if (!read_disk_block(
                physical_block,
                data_buffer))
        {
            return false;
        }

        size_t remaining =
            wanted - total_read;

        size_t chunk =
            (remaining < block_size)
                ? remaining
                : block_size;

        for (size_t p = 0;
             p < chunk;
             ++p)
        {
            dest[total_read + p] =
                data_buffer[p];
        }

        total_read += chunk;
    }

    return total_read > 0;
}


/* ============================================================
 * Könyvtárbejegyzések keresése
 *
 * EXT4 directory entry:
 *
 * uint32 inode
 * uint16 rec_len
 * uint8  name_len
 * uint8  file_type
 * char   name[]
 * ============================================================ */

static uint32_t find_inode_in_directory(
    Ext4Reader& fs,
    uint32_t directory_inode,
    const char* component)
{
    if (directory_inode == 0)
        return 0;

    if (!component)
        return 0;

    /*
     * A jelenlegi Ext4Reader API-ban a get_inode()
     * privát. Ezért ezt a keresést nem innen
     * végezzük.
     *
     * Ez a helper csak azért marad, hogy a forrás
     * kompatibilis legyen a korábbi felépítéssel.
     */
    (void)fs;
    (void)directory_inode;
    (void)component;

    return 0;
}


/* ============================================================
 * Útvonal feldolgozása
 * ============================================================ */

uint32_t Ext4Reader::find_file_inode(
    const char* path)
{
    if (!path)
        return 0;

    if (path[0] == '\0')
        return 0;

    /*
     * A root inode EXT4-ben:
     * 2
     */
    uint32_t current_inode = 2;

    /*
     * "/" maga a root directory.
     */
    if (string_equals(path, "/"))
        return current_inode;

    /*
     * A jelenlegi egyszerű EXT4 loaderben
     * a teljes könyvtárbejárás csak akkor végezhető
     * el, ha az inode directory adatait ugyanebben
     * a tagfüggvényben olvassuk.
     */

    size_t path_len =
        string_length(path);

    if (path_len == 0)
        return 0;

    size_t pos = 0;

    /*
     * Kezdő "/" átugrása.
     */
    if (path[0] == '/')
        pos = 1;

    while (pos < path_len)
    {
        /*
         * Következő komponens kezdete.
         */
        size_t component_start = pos;

        while (pos < path_len &&
               path[pos] != '/')
        {
            ++pos;
        }

        size_t component_len =
            pos - component_start;

        /*
         * Üres komponens kihagyása.
         */
        if (component_len == 0)
        {
            while (pos < path_len &&
                   path[pos] == '/')
            {
                ++pos;
            }

            continue;
        }

        /*
         * Komponens maximum 255 karakter.
         */
        if (component_len > 255)
            return 0;

        char component[256];

        for (size_t i = 0;
             i < component_len;
             ++i)
        {
            component[i] =
                path[component_start + i];
        }

        component[component_len] = '\0';

        /*
         * Aktuális inode beolvasása.
         */
        Ext4Inode directory_inode;

        if (!get_inode(
                current_inode,
                directory_inode))
        {
            return 0;
        }

        /*
         * Csak könyvtárból lehet következő
         * útvonal-komponenst keresni.
         */
        if ((directory_inode.mode & 0xF000) !=
            0x4000)
        {
            return 0;
        }

        uint64_t directory_size =
            static_cast<uint64_t>(
                directory_inode.size_lo);

        if (directory_inode.size_high != 0)
        {
            directory_size |=
                static_cast<uint64_t>(
                    directory_inode.size_high) << 32;
        }

        /*
         * Egyszerű, biztonságos buffer.
         *
         * Nagy könyvtáraknál chunkokban dolgozunk.
         */
        alignas(4096) uint8_t block_buffer[4096];

        size_t directory_offset = 0;

        bool found = false;
        uint32_t next_inode = 0;

        /*
         * Első 12 direkt blokk.
         *
         * Kis EXT4 könyvtárakhoz elegendő.
         */
        const uint32_t* direct_blocks =
            reinterpret_cast<const uint32_t*>(
                directory_inode.block
            );

        for (uint32_t block_index = 0;
             block_index < 12;
             ++block_index)
        {
            if (directory_offset >= directory_size)
                break;

            uint32_t physical_block =
                direct_blocks[block_index];

            if (physical_block == 0)
                continue;

            if (!read_disk_block(
                    physical_block,
                    block_buffer))
            {
                return 0;
            }

            size_t bytes_in_block =
                block_size;

            if (directory_size -
                    directory_offset <
                bytes_in_block)
            {
                bytes_in_block =
                    static_cast<size_t>(
                        directory_size -
                        directory_offset);
            }

            size_t offset = 0;

            while (offset + 8 <= bytes_in_block)
            {
                const uint8_t* entry =
                    block_buffer + offset;

                uint32_t inode =
                    read_u32(entry);

                uint16_t rec_len =
                    read_u16(entry + 4);

                uint8_t name_len =
                    entry[6];

                /*
                 * Érvénytelen directory entry
                 * ellenőrzése.
                 */
                if (rec_len < 8)
                    break;

                if ((rec_len & 3) != 0)
                    break;

                if (offset + rec_len >
                    bytes_in_block)
                {
                    break;
                }

                if (name_len >
                    rec_len - 8)
                {
                    break;
                }

                /*
                 * inode == 0:
                 * törölt/üres directory entry.
                 */
                if (inode != 0)
                {
                    const char* name =
                        reinterpret_cast<const char*>(
                            entry + 8
                        );

                    if (path_component_equal(
                            name,
                            name_len,
                            component,
                            component_len))
                    {
                        next_inode = inode;
                        found = true;
                        break;
                    }
                }

                offset += rec_len;
            }

            if (found)
                break;

            directory_offset +=
                bytes_in_block;
        }

        if (!found)
            return 0;

        current_inode = next_inode;

        /*
         * Következő "/" átugrása.
         */
        while (pos < path_len &&
               path[pos] == '/')
        {
            ++pos;
        }
    }

    return current_inode;
}


/* ============================================================
 * Globális EXT4 példány
 *
 * NINCS konstruktor-definíció itt!
 * ============================================================ */

Ext4Reader filesystem;
