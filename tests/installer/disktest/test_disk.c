/* Host test for userspace/installer/disk_install.c: disks are files.
 * disk 0 = source (read-only here, like the kernel), disk 1 = target, disk 2 absent. */
#define DISK_HOST_TEST
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

enum { OP_COUNT = 0, OP_SECTORS = 1, OP_READ = 2, OP_WRITE = 3, OP_FLUSH = 4 };
static int fds[3] = {-1, -1, -1};
static long nwrites[3];

static long bdisk(long op, long disk, uint64_t lba, uint64_t count, void *buf) {
    if (disk < 0 || disk > 2 || fds[disk] < 0) return -19;          /* ENODEV */
    off_t off = (off_t)lba * 512;
    switch (op) {
    case OP_SECTORS: { off_t sz = lseek(fds[disk], 0, SEEK_END); return sz / 512; }
    case OP_READ:
        if (pread(fds[disk], buf, count * 512, off) != (ssize_t)(count * 512)) return -5;
        return 0;
    case OP_WRITE:
        if (disk == 0) return -1;                                   /* kernel refuses boot disk */
        if (count == 0 || count > 255) return -22;
        if (pwrite(fds[disk], buf, count * 512, off) != (ssize_t)(count * 512)) return -5;
        nwrites[disk]++;
        return 0;
    case OP_FLUSH: fsync(fds[disk]); return 0;
    }
    return -22;
}
void machine_reboot(void) { }

#include "../../../userspace/installer/disk_install.c"

static void prog(int step, int pct) { if (pct == 0 || pct == 100) printf("  step %d %d%%\n", step, pct); }

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: test_disk src.img dst.img\n"); return 2; }
    fds[0] = open(argv[1], O_RDWR);
    fds[1] = open(argv[2], O_RDWR);
    if (fds[0] < 0 || fds[1] < 0) { perror("open"); return 2; }
    int src = disk_find_source();
    printf("source disk: %d (valid=%d cluster=%u size=%u)\n", src, g_src.valid, g_src.cluster, g_src.size);
    struct DiskInfo c[2];
    int n = disk_candidates(c, 2);
    printf("candidates: %d", n);
    for (int i = 0; i < n; ++i) printf(" [disk %d, %llu sectors]", c[i].index, (unsigned long long)c[i].sectors);
    printf("\n");
    int rc = disk_install(1, prog);
    printf("disk_install -> %d (writes to target %ld)\n", rc, nwrites[1]);
    return rc ? 1 : 0;
}
