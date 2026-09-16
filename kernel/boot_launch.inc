/*
 * boot_launch.inc — a drop-in replacement for the "First userspace program"
 * block near the END of kernel/kernel.cpp (the part that currently reads
 * /bin/sh, calls process::create(), then process::run()).
 *
 * HOW TO APPLY
 * ------------
 * In kernel/kernel.cpp, find this block (it sits right after the splash
 * screen's key-wait loop):
 *
 *     uint32_t shell_size = 0;
 *     const uint8_t* shell_elf = vfs::read_file("/bin/sh", &shell_size);
 *     ... if (!shell_elf || shell_size == 0) { halt }
 *     process::Process* shell_process = process::create(shell_elf, (size_t)shell_size);
 *     ... if (!shell_process) { halt }
 *     process::run(shell_process);
 *     while (1) { __asm__ volatile("cli; hlt"); }
 *
 * ...and replace it ENTIRELY with the code below (or #include this file
 * there). Make sure kernel.cpp has `#include "process.hpp"` — it already
 * does for the existing process::create call.
 *
 * WHAT CHANGES
 * ------------
 * Before: ONE process is loaded and run; the kernel blocks until it exits.
 * After:  every program listed in boot_programs[] is spawned, and the
 *         preemptive scheduler runs them CONCURRENTLY, returning only once
 *         all of them have exited.
 *
 * Keeping /bin/sh first in the list preserves the previous behavior when
 * the test programs are absent: if only /bin/sh exists, this runs exactly
 * one process, just as before.
 */

{
    /*
     * Programs to start at boot, in order. A missing file is skipped with
     * a message rather than being fatal, so a rootfs that only has some of
     * these still boots — useful while you're building the image up.
     */
    static const char* const boot_programs[] = {
        "/bin/sh",
        "/bin/proc_a",
        "/bin/proc_b",
    };
    static const size_t boot_program_count =
        sizeof(boot_programs) / sizeof(boot_programs[0]);

    process::init();

    size_t spawned = 0;

    for (size_t i = 0; i < boot_program_count; i++) {
        uint32_t prog_size = 0;
        const uint8_t* prog_elf = vfs::read_file(boot_programs[i], &prog_size);

        if (!prog_elf || prog_size == 0) {
            printf("boot: skipping %s (not found in rootfs)\n", boot_programs[i]);
            continue;
        }

        process::Process* proc =
            process::spawn(prog_elf, (size_t)prog_size, boot_programs[i]);

        if (!proc) {
            /* Most likely causes, in rough order of likelihood:
             *   - the binary is dynamically linked but /system/lib/ld.so is
             *     missing from the rootfs (see userspace/ldso/README.md)
             *   - it's PIE; the loader path here expects ET_EXEC (-no-pie)
             *   - out of memory / no free process slot */
            printf("boot: FAILED to spawn %s\n", boot_programs[i]);
            continue;
        }

        printf("boot: spawned %s as pid %llu\n",
               boot_programs[i], (unsigned long long)proc->pid);
        spawned++;
    }

    if (spawned == 0) {
        printf("boot: no userspace program could be started, halting\n");
        while (1) {
            __asm__ volatile("cli; hlt");
        }
    }

    printf("boot: starting scheduler with %llu process(es)\n",
           (unsigned long long)spawned);

    /*
     * Enters ring3 and round-robins between every spawned task on each
     * timer tick. Returns only once they have ALL exited.
     */
    process::run_scheduler();

    printf("boot: all userspace processes have exited\n");

    while (1) {
        __asm__ volatile("cli; hlt");
    }
}
