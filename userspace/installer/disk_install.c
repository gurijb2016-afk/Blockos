/* BlockOS installer: disk stage.
 *
 *   disk_find_source()  finds the disk BOOTX64.EFI was booted from (\EFI\BOOT\BOOTX64.EFI on a
 *                       FAT32 partition: GPT, MBR or an unpartitioned FAT volume)
 *   disk_install()      writes a GPT (EFI System 128 MiB + BlockOS system partition), formats the
 *                       EFI partition as FAT32 and streams BOOTX64.EFI from the source disk onto it
 *   machine_reboot()    keyboard-controller reset
 *
 * All disk access is the blockos_disk syscall (kernel/user_syscall.cpp): op 1 sector count,
 * 2 read, 3 write (never the boot disk, index 0), 4 flush. The FAT32 layout is fixed: one
 * sector per cluster, 32 reserved sectors, 2 FATs, root directory at cluster 2,
 * \EFI at cluster 3, \EFI\BOOT at cluster 4, BOOTX64.EFI from cluster 5 on, contiguous.
 *
 * Build for the host test with -DDISK_HOST_TEST and define bdisk()/machine_reboot() first.
 */
#include "disk_install.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifndef DISK_HOST_TEST
#include "blockos_syscall.h"

enum { OP_COUNT = 0, OP_SECTORS = 1, OP_READ = 2, OP_WRITE = 3, OP_FLUSH = 4 };

static long bdisk(long op, long disk, uint64_t lba, uint64_t count, void *buf) {
    return __blockos_syscall(__SYS_blockos_disk, op, disk, (long)lba, (long)count, (long)(uintptr_t)buf, 0);
}

void machine_reboot(void) {
    __blockos_syscall(__SYS_blockos_reboot, 0, 0, 0, 0, 0, 0);
    for (;;) {}
}
#endif

#define SECT 512u
#define CHUNK 255u                       /* sectors per disk call (the kernel accepts 1..255) */
#define ESP_START 2048ull
#define ESP_SECTORS 262144ull            /* 128 MiB */
#define SYS_START (ESP_START + ESP_SECTORS)
#define MIN_DISK (SYS_START + 2048ull + 34ull)
#define FAT_RSVD 32u
#define FAT_NFATS 2u
#define FILE_CLUSTER0 5u                 /* BOOTX64.EFI starts here: 2 root, 3 EFI, 4 EFI\BOOT */

/* GPT type GUIDs in on-disk (mixed-endian) byte order */
static const uint8_t GUID_ESP[16]  = {0x28,0x73,0x2A,0xC1,0x1F,0xF8,0xD2,0x11,0xBA,0x4B,0x00,0xA0,0xC9,0x3E,0xC9,0x3B};
static const uint8_t GUID_DATA[16] = {0xA2,0xA0,0xD0,0xEB,0xE5,0xB9,0x33,0x44,0x87,0xC0,0x68,0xB6,0xB7,0x26,0x99,0xC7};

static uint8_t g_io[CHUNK * SECT];
static uint8_t g_ent[128 * 128];        /* GPT entry array: 128 entries of 128 bytes */
static uint8_t g_fat_cache[SECT];
static uint64_t g_fat_cache_lba = ~0ull;
static int g_fat_cache_disk = -1;
static uint64_t g_rng;

struct Fat { int disk; uint64_t part; uint32_t spc, rsvd, nfats, fatsz, rootclus; uint64_t data; };
static struct {
    int valid;
    int disk;
    struct Fat fat;
    uint32_t cluster;   /* first cluster of BOOTX64.EFI on the source */
    uint32_t size;      /* bytes */
} g_src;

/* ---------- little-endian helpers ---------- */

static void put16(uint8_t *b, uint32_t off, uint32_t v) { b[off] = (uint8_t)v; b[off + 1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *b, uint32_t off, uint32_t v) { for (int i = 0; i < 4; ++i) b[off + i] = (uint8_t)(v >> (8 * i)); }
static void put64(uint8_t *b, uint32_t off, uint64_t v) { for (int i = 0; i < 8; ++i) b[off + i] = (uint8_t)(v >> (8 * i)); }
static uint16_t get16(const uint8_t *b, uint32_t off) { return (uint16_t)(b[off] | (b[off + 1] << 8)); }
static uint32_t get32(const uint8_t *b, uint32_t off) {
    return (uint32_t)b[off] | ((uint32_t)b[off + 1] << 8) | ((uint32_t)b[off + 2] << 16) | ((uint32_t)b[off + 3] << 24);
}
static uint64_t get64(const uint8_t *b, uint32_t off) { return get32(b, off) | ((uint64_t)get32(b, off + 4) << 32); }

static uint32_t crc32(const uint8_t *p, size_t n) {
    uint32_t c = 0xFFFFFFFFu;
    while (n--) {
        c ^= *p++;
        for (int k = 0; k < 8; ++k) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}

static void rand_bytes(uint8_t *out, size_t n) {
    if (!g_rng) g_rng = ((uint64_t)time(NULL) * 0x9E3779B97F4A7C15ull) | 1u;
    for (size_t i = 0; i < n; ++i) {
        g_rng ^= g_rng << 13; g_rng ^= g_rng >> 7; g_rng ^= g_rng << 17;
        out[i] = (uint8_t)(g_rng >> 24);
    }
}

/* ---------- disk I/O in chunks of at most CHUNK sectors ---------- */

static int rd(int d, uint64_t lba, uint32_t n, void *buf) {
    uint8_t *p = buf;
    while (n) {
        uint32_t c = n > CHUNK ? CHUNK : n;
        if (bdisk(OP_READ, d, lba, c, p) != 0) return -1;
        lba += c; p += (size_t)c * SECT; n -= c;
    }
    return 0;
}

static int wr(int d, uint64_t lba, uint32_t n, const void *buf) {
    const uint8_t *p = buf;
    while (n) {
        uint32_t c = n > CHUNK ? CHUNK : n;
        if (bdisk(OP_WRITE, d, lba, c, (void *)p) != 0) return -1;
        lba += c; p += (size_t)c * SECT; n -= c;
    }
    return 0;
}

static void report(disk_progress_fn p, int step, int pct) { if (p) p(step, pct); }

/* ---------- source: find \EFI\BOOT\BOOTX64.EFI ---------- */

/* 8.3 name, space padded: "EFI" -> "EFI        ", "BOOTX64"/"EFI" -> "BOOTX64EFI" split as base+ext */
static void name11(uint8_t *o, const char *base, const char *ext) {
    memset(o, ' ', 11);
    memcpy(o, base, strlen(base));
    memcpy(o + 8, ext, strlen(ext));
}

static int fat_open(int d, uint64_t part, struct Fat *f) {
    uint8_t s[SECT];
    if (rd(d, part, 1, s)) return -1;
    if (s[510] != 0x55 || s[511] != 0xAA || get16(s, 11) != 512) return -1;
    if (memcmp(s + 82, "FAT32   ", 8) != 0) return -1;
    if (s[13] == 0) return -1;
    f->disk = d;
    f->part = part;
    f->spc = s[13];
    f->rsvd = get16(s, 14);
    f->nfats = s[16];
    f->fatsz = get32(s, 36);
    f->rootclus = get32(s, 44);
    f->data = part + f->rsvd + (uint64_t)f->nfats * f->fatsz;
    return 0;
}

static uint32_t fat_next(const struct Fat *f, uint32_t c) {
    const uint64_t off = (uint64_t)c * 4u;
    const uint64_t lba = f->part + f->rsvd + off / SECT;
    if (g_fat_cache_lba != lba || g_fat_cache_disk != f->disk) {
        if (rd(f->disk, lba, 1, g_fat_cache)) return 0x0FFFFFFFu;
        g_fat_cache_lba = lba;
        g_fat_cache_disk = f->disk;
    }
    return get32(g_fat_cache, (uint32_t)(off % SECT)) & 0x0FFFFFFFu;
}

/* Looks up one short name in the directory starting at cluster. */
static int dir_find(const struct Fat *f, uint32_t cluster, const char *base, const char *ext,
                    uint32_t *out_cluster, uint32_t *out_size, int *is_dir) {
    uint8_t name[11];
    uint8_t s[SECT];
    uint32_t guard = 0;
    name11(name, base, ext);
    while (cluster >= 2 && cluster < 0x0FFFFFF0u && guard++ < 1000000u) {
        for (uint32_t k = 0; k < f->spc; ++k) {
            if (rd(f->disk, f->data + (uint64_t)(cluster - 2) * f->spc + k, 1, s)) return -1;
            for (uint32_t e = 0; e < SECT / 32; ++e) {
                const uint8_t *de = s + 32 * e;
                if (de[0] == 0) return -1;                       /* end of directory */
                if (de[0] == 0xE5 || (de[11] & 0x0F) == 0x0F) continue;   /* deleted or long name */
                if (memcmp(de, name, 11) == 0) {
                    *out_cluster = ((uint32_t)get16(de, 20) << 16) | get16(de, 26);
                    *out_size = get32(de, 28);
                    *is_dir = (de[11] & 0x10) != 0;
                    return 0;
                }
            }
        }
        cluster = fat_next(f, cluster);
    }
    return -1;
}

static int find_on_partition(int d, uint64_t start) {
    struct Fat f;
    uint32_t c1 = 0, c2 = 0, c3 = 0, sz = 0;
    int dir = 0;
    if (fat_open(d, start, &f)) return -1;
    if (dir_find(&f, f.rootclus, "EFI", "", &c1, &sz, &dir) || !dir) return -1;
    if (dir_find(&f, c1, "BOOT", "", &c2, &sz, &dir) || !dir) return -1;
    if (dir_find(&f, c2, "BOOTX64", "EFI", &c3, &sz, &dir) || dir) return -1;
    g_src.valid = 1;
    g_src.disk = d;
    g_src.fat = f;
    g_src.cluster = c3;
    g_src.size = sz;
    return 0;
}

int disk_find_source(void) {
    uint8_t s0[SECT], hdr[SECT];
    g_src.valid = 0;
    g_fat_cache_lba = ~0ull;
    for (int d = 0; d < 3; ++d) {
        if ((int64_t)bdisk(OP_SECTORS, d, 0, 0, 0) <= 0) continue;
        if (rd(d, 0, 1, s0)) continue;
        if (rd(d, 1, 1, hdr) == 0 && memcmp(hdr, "EFI PART", 8) == 0) {
            const uint32_t n = get32(hdr, 80), esz = get32(hdr, 84);
            if (esz != 128 || n == 0 || n > 128) continue;
            if (rd(d, get64(hdr, 72), 32, g_ent)) continue;
            for (uint32_t i = 0; i < n; ++i) {
                const uint8_t *e = g_ent + 128 * i;
                if (get64(e, 32) == 0) continue;
                if (find_on_partition(d, get64(e, 32)) == 0) return d;
            }
            continue;
        }
        if (s0[510] == 0x55 && s0[511] == 0xAA) {          /* MBR */
            for (int i = 0; i < 4; ++i) {
                const uint8_t *e = s0 + 446 + 16 * i;
                if (e[4] == 0 || get32(e, 8) == 0) continue;
                if (find_on_partition(d, get32(e, 8)) == 0) return d;
            }
        }
        if (find_on_partition(d, 0) == 0) return d;        /* unpartitioned FAT volume */
    }
    return -1;
}

/* ---------- candidates ---------- */

int disk_candidates(struct DiskInfo *out, int max) {
    int n = 0;
    for (int d = 1; d < 3 && n < max; ++d) {   /* disk 0 is the boot disk: the kernel never writes it */
        if (g_src.valid && d == g_src.disk) continue;
        const int64_t s = (int64_t)bdisk(OP_SECTORS, d, 0, 0, 0);
        if (s < (int64_t)MIN_DISK) continue;
        out[n].index = d;
        out[n].sectors = (uint64_t)s;
        ++n;
    }
    return n;
}

/* ---------- target: GPT ---------- */

static void gpt_entry(uint8_t *e, const uint8_t type[16], uint64_t first, uint64_t last, const char *name) {
    memset(e, 0, 128);
    memcpy(e, type, 16);
    rand_bytes(e + 16, 16);
    put64(e, 32, first);
    put64(e, 40, last);
    for (uint32_t i = 0; name[i]; ++i) e[56 + 2 * i] = (uint8_t)name[i];   /* UTF-16LE, ASCII only */
}

static void gpt_header(uint8_t *h, uint64_t my, uint64_t alt, uint64_t ent_lba, uint64_t total,
                       const uint8_t dguid[16], uint32_t ent_crc) {
    memset(h, 0, SECT);
    memcpy(h, "EFI PART", 8);
    put32(h, 8, 0x00010000u);
    put32(h, 12, 92);
    put64(h, 24, my);
    put64(h, 32, alt);
    put64(h, 40, 34);
    put64(h, 48, total - 34);
    memcpy(h + 56, dguid, 16);
    put64(h, 72, ent_lba);
    put32(h, 80, 128);
    put32(h, 84, 128);
    put32(h, 88, ent_crc);
    put32(h, 16, crc32(h, 92));
}

static int gpt_write(int d, uint64_t total) {
    uint8_t mbr[SECT], hdr[SECT], dguid[16];
    const uint64_t last = total - 1;
    memset(g_ent, 0, sizeof g_ent);
    rand_bytes(dguid, 16);
    gpt_entry(g_ent, GUID_ESP, ESP_START, ESP_START + ESP_SECTORS - 1, "EFI System");
    gpt_entry(g_ent + 128, GUID_DATA, SYS_START, total - 34, "BlockOS");
    const uint32_t ent_crc = crc32(g_ent, sizeof g_ent);

    /* protective MBR: one 0xEE partition covering the disk */
    memset(mbr, 0, SECT);
    mbr[450] = 0xEE;
    mbr[451] = mbr[452] = mbr[453] = 0xFF;
    put32(mbr, 454, 1);
    put32(mbr, 458, last > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)last);
    mbr[510] = 0x55;
    mbr[511] = 0xAA;

    if (wr(d, 0, 1, mbr)) return -1;
    if (wr(d, 2, 32, g_ent)) return -1;                       /* primary entries */
    gpt_header(hdr, 1, last, 2, total, dguid, ent_crc);
    if (wr(d, 1, 1, hdr)) return -1;
    if (wr(d, total - 33, 32, g_ent)) return -1;              /* backup entries */
    gpt_header(hdr, last, 1, total - 33, total, dguid, ent_crc);
    if (wr(d, last, 1, hdr)) return -1;

    /* clear the first sector of the system partition: no old filesystem may be seen there */
    memset(g_io, 0, SECT);
    return wr(d, SYS_START, 1, g_io);
}

/* ---------- target: FAT32 on the EFI partition ---------- */

static uint32_t fat_sectors_per_fat(uint64_t part_sectors) {
    /* one cluster = one sector; need fatsz * 128 entries >= clusters + 2 */
    uint64_t fs = 1;
    for (;;) {
        const uint64_t clusters = part_sectors - FAT_RSVD - FAT_NFATS * fs;
        if (fs * 128u >= clusters + 2u) return (uint32_t)fs;
        ++fs;
    }
}

static uint32_t fat_value(uint32_t e, uint32_t nfile) {
    if (e == 0) return 0x0FFFFFF8u;                              /* media */
    if (e <= 4) return 0x0FFFFFFFu;                              /* 1 reserved, 2 root, 3 EFI, 4 EFI\BOOT */
    if (e < FILE_CLUSTER0 + nfile - 1) return e + 1;             /* BOOTX64.EFI chain */
    if (e == FILE_CLUSTER0 + nfile - 1) return 0x0FFFFFFFu;      /* last cluster of the file */
    return 0;
}

static void dirent(uint8_t *e, const char *base, const char *ext, uint8_t attr, uint32_t cluster, uint32_t size) {
    memset(e, 0, 32);
    name11(e, base, ext);
    e[11] = attr;
    put16(e, 20, cluster >> 16);
    put16(e, 26, cluster & 0xFFFFu);
    put32(e, 28, size);
}

static int fat_format_esp(int d, uint32_t fatsz, uint32_t nfile, uint32_t file_size) {
    uint8_t s[SECT], fi[SECT];

    memset(s, 0, SECT);
    s[0] = 0xEB; s[1] = 0x58; s[2] = 0x90;
    memcpy(s + 3, "BLOCKOS ", 8);
    put16(s, 11, 512);
    s[13] = 1;                                   /* sectors per cluster */
    put16(s, 14, FAT_RSVD);
    s[16] = FAT_NFATS;
    s[21] = 0xF8;
    put16(s, 24, 63);
    put16(s, 26, 255);
    put32(s, 28, (uint32_t)ESP_START);           /* hidden sectors */
    put32(s, 32, (uint32_t)ESP_SECTORS);
    put32(s, 36, fatsz);
    put32(s, 44, 2);                             /* root cluster */
    put16(s, 48, 1);                             /* FSInfo sector */
    put16(s, 50, 6);                             /* backup boot sector */
    s[64] = 0x80;
    s[66] = 0x29;
    put32(s, 67, 0x424F4C4Bu);                   /* volume id */
    memcpy(s + 71, "BLOCKOS    ", 11);
    memcpy(s + 82, "FAT32   ", 8);
    s[510] = 0x55; s[511] = 0xAA;
    if (wr(d, ESP_START, 1, s)) return -1;
    if (wr(d, ESP_START + 6, 1, s)) return -1;

    memset(fi, 0, SECT);
    put32(fi, 0, 0x41615252u);
    put32(fi, 484, 0x61417272u);
    {
        /* data clusters 2..; used: root, EFI, EFI\BOOT and the file's clusters */
        const uint32_t clusters = ESP_SECTORS - FAT_RSVD - 2u * fatsz;
        put32(fi, 488, clusters - 3u - nfile);   /* free clusters */
        put32(fi, 492, FILE_CLUSTER0 + nfile);   /* next free cluster */
    }
    put32(fi, 508, 0xAA550000u);
    if (wr(d, ESP_START + 1, 1, fi)) return -1;
    if (wr(d, ESP_START + 7, 1, fi)) return -1;

    memset(g_io, 0, 24 * SECT);                  /* the rest of the reserved area */
    if (wr(d, ESP_START + 2, 4, g_io)) return -1;
    if (wr(d, ESP_START + 8, 24, g_io)) return -1;

    /* both FATs, written in blocks */
    for (uint32_t i0 = 0; i0 < fatsz; i0 += CHUNK) {
        const uint32_t n = fatsz - i0 < CHUNK ? fatsz - i0 : CHUNK;
        for (uint32_t i = 0; i < n; ++i)
            for (uint32_t j = 0; j < 128; ++j)
                put32(g_io, i * SECT + 4 * j, fat_value((i0 + i) * 128u + j, nfile));
        if (wr(d, ESP_START + FAT_RSVD + i0, n, g_io)) return -1;
        if (wr(d, ESP_START + FAT_RSVD + fatsz + i0, n, g_io)) return -1;
    }

    /* directories: cluster 2 (root), 3 (\EFI), 4 (\EFI\BOOT), at the start of the data area */
    {
        const uint64_t data = ESP_START + FAT_RSVD + 2ull * fatsz;
        uint8_t *root = g_io, *efi = g_io + SECT, *boot = g_io + 2 * SECT;
        memset(g_io, 0, 3 * SECT);
        dirent(root, "EFI", "", 0x10, 3, 0);
        dirent(root + 32, "BLOCKOS", "", 0x08, 0, 0);   /* volume label, matches the boot sector */
        dirent(efi, ".", "", 0x10, 3, 0);
        dirent(efi + 32, "..", "", 0x10, 0, 0);
        dirent(efi + 64, "BOOT", "", 0x10, 4, 0);
        dirent(boot, ".", "", 0x10, 4, 0);
        dirent(boot + 32, "..", "", 0x10, 3, 0);
        dirent(boot + 64, "BOOTX64", "EFI", 0x20, FILE_CLUSTER0, file_size);
        if (wr(d, data, 3, g_io)) return -1;
    }
    return 0;
}

/* ---------- copy BOOTX64.EFI from the source FAT to the target ---------- */

static int copy_file(uint64_t dst_lba, int tdisk, disk_progress_fn prog) {
    const struct Fat *f = &g_src.fat;
    uint64_t left = ((uint64_t)g_src.size + SECT - 1) / SECT;   /* sectors to copy */
    const uint64_t total = left;
    uint64_t done = 0;
    uint32_t fill = 0;                       /* sectors buffered in g_io */
    uint32_t c = g_src.cluster;
    uint32_t guard = 0;

    while (left) {
        if (c < 2 || c >= 0x0FFFFFF0u || guard++ > 0x1000000u) return -1;   /* broken chain */
        const uint32_t take = f->spc < left ? f->spc : (uint32_t)left;
        if (fill + take > CHUNK) {
            if (wr(tdisk, dst_lba + done, fill, g_io)) return -2;
            done += fill; fill = 0;
            report(prog, 2, (int)(done * 100 / total));
        }
        if (rd(f->disk, f->data + (uint64_t)(c - 2) * f->spc, take, g_io + (size_t)fill * SECT)) return -1;
        fill += take;
        left -= take;
        if (left == 0 || fill == CHUNK) {
            if (wr(tdisk, dst_lba + done, fill, g_io)) return -2;
            done += fill; fill = 0;
            report(prog, 2, (int)(done * 100 / total));
        }
        c = fat_next(f, c);
    }
    report(prog, 2, 100);
    return 0;
}

/* ---------- install ---------- */

int disk_install(int target, disk_progress_fn prog) {
    if (!g_src.valid) return -10;
    if (target == 0 || (g_src.valid && target == g_src.disk)) return -11;

    const int64_t s = (int64_t)bdisk(OP_SECTORS, target, 0, 0, 0);
    if (s <= 0) return -13;
    const uint64_t total = (uint64_t)s;
    if (total < MIN_DISK) return -12;

    const uint64_t nfile = ((uint64_t)g_src.size + SECT - 1) / SECT;
    const uint32_t fatsz = fat_sectors_per_fat(ESP_SECTORS);
    const uint64_t clusters = ESP_SECTORS - FAT_RSVD - 2ull * fatsz;
    if (nfile == 0 || nfile + 3 > clusters) return -12;

    report(prog, 0, 0);
    if (gpt_write(target, total)) return -20;
    report(prog, 0, 100);

    report(prog, 1, 0);
    if (fat_format_esp(target, fatsz, (uint32_t)nfile, g_src.size)) return -21;
    report(prog, 1, 100);

    report(prog, 2, 0);
    const uint64_t dst = ESP_START + FAT_RSVD + 2ull * fatsz + (FILE_CLUSTER0 - 2);
    if (copy_file(dst, target, prog)) return -22;

    report(prog, 3, 0);
    if (bdisk(OP_FLUSH, target, 0, 0, 0) != 0) return -23;
    report(prog, 3, 100);
    return 0;
}
