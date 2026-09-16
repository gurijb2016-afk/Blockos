/*
 * proc_b.c - second of two concurrently-running test processes.
 *
 * What this proves when you see its output interleaved with proc_a's:
 *   - the ELF loader mapped it into its own address space
 *   - crt1.S found argc/argv/envp on the kernel-built stack
 *   - the libc (printf -> vsnprintf -> write syscall) works
 *   - the PREEMPTIVE SCHEDULER is actually switching between two ring3
 *     tasks: without it you would see all of A's lines, then all of B's.
 *
 * Interleaved output = real preemption. Sequential output = the timer
 * switch is not firing, and something in the scheduler wiring is wrong.
 */
#include <stdio.h>
#include <unistd.h>

int main(int argc, char** argv, char** envp) {
    (void)argv; (void)envp;

    printf("[B] started, argc=%d, pid=%d\n", argc, getpid());

    for (int i = 0; i < 20; i++) {
        printf("[B] tick %d\n", i);
        /* Busy-spin long enough that the 100 Hz timer is very likely to
         * preempt us mid-loop. No sleep() is used because that would need
         * a blocking syscall, which the current scheduler deliberately
         * does not support yet (see preempt.cpp's note on the shared
         * kernel stack). */
        for (volatile long spin = 0; spin < 8000000L; spin++) { }
    }

    printf("[B] done\n");
    return 0;
}
