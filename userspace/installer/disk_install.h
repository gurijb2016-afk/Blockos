/* BlockOS installer: disk stage. See disk_install.c. */
#pragma once
#include <stdint.h>

struct DiskInfo {
    int index;          /* kernel disk index (1 = Primary Slave, 2 = Secondary Master) */
    uint64_t sectors;   /* 512-byte sectors */
};

/* step 0..3 (partition table, format, copy, flush); percent 0..100 inside the step */
typedef void (*disk_progress_fn)(int step, int percent);

/* Finds the disk BOOTX64.EFI was booted from (\EFI\BOOT\BOOTX64.EFI on a FAT32 partition).
 * Returns the disk index, or -1. */
int disk_find_source(void);

/* Disks that can take the installation (not the boot disk, not the source, big enough). */
int disk_candidates(struct DiskInfo *out, int max);

/* Partitions target (GPT: EFI System 128 MiB + BlockOS system), formats the EFI partition as
 * FAT32, copies BOOTX64.EFI to \EFI\BOOT\BOOTX64.EFI and flushes.
 * Returns 0, or a negative code: -10 no source, -11 bad target, -12 target too small
 * (or too small for the EFI file), -13 target disk missing, -20 GPT write, -21 format,
 * -22 copy, -23 flush. */
int disk_install(int target, disk_progress_fn progress);

/* Resets the machine (keyboard controller reset line). Does not return. */
void machine_reboot(void);
