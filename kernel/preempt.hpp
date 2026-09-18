#pragma once
#include <stdint.h>
#include <stddef.h>

/*
 * Canonical saved ring3 state.
 *
 * Both kernel entry paths already push exactly these registers in exactly
 * this order before calling their C handler, and both restore from the
 * same memory and `iretq` afterwards:
 *
 *   - arch/86_64x/irq_stubs.S  (isr_common)        -> interrupt_handler_c
 *   - arch/86_64x/syscall_entry.S                  -> blockos_syscall_dispatch_frame
 *
 * That means a task switch needs NO new assembly: the C handler overwrites
 * the frame in place with another task's saved state, switches CR3, and the
 * existing `iretq` lands in the other task. The only difference between the
 * two entry paths is that the interrupt stub also pushes vector+error_code
 * between r15 and rip, so each path converts to/from this struct.
 */
struct TrapFrame {
    uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t rip, cs, rflags, rsp, ss;
};

/* Layout as pushed by irq_stubs.S / read by interrupt_handler_c. */
struct InterruptFrame {
    uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t vector, error_code;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed));

/* Layout as pushed by syscall_entry.S (no vector/error_code). */
struct BlockOSSyscallFrame {
    uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t rip, cs, rflags, rsp, ss;
};

namespace preempt {

/*
 * Called from interrupt_handler_c on every timer tick (vector 32), AFTER
 * pit_handler_c()/EOI. Returns true if it switched tasks (the frame now
 * holds a different task's state and CR3 has been changed).
 *
 * Does nothing and returns false when the interrupted code was running in
 * ring 0 (cs & 3 == 0): preempting the kernel itself would be unsafe here,
 * because every task shares one kernel stack (tss.rsp0) - see the note in
 * preempt.cpp.
 */
bool on_timer(InterruptFrame* f);

/*
 * Called from the SYS_exit/SYS_exit_group path. Marks the current task
 * terminated and, if another task is READY, loads it into the frame and
 * switches CR3 - returning true. Returns false when no other task is
 * runnable, in which case the caller falls back to returning into the
 * kernel (the original single-process behavior).
 */
bool on_exit(BlockOSSyscallFrame* f);

/*
 * Voluntary yield from a syscall (used by the IPC layer when a task blocks
 * waiting for a message). Behaves like on_timer but for the syscall frame
 * layout, and only switches if another task is READY.
 */
bool yield_from_syscall(BlockOSSyscallFrame* f);

/* Block the current task at this syscall instruction and switch directly
 * to another READY task. The blocked task is resumed later from the same
 * saved frame with whatever return value the caller placed in rax. */
bool block_from_syscall(BlockOSSyscallFrame* f);
bool block_until_from_syscall(BlockOSSyscallFrame* f, uint64_t deadline_ms);

void to_trap(const InterruptFrame* src, TrapFrame* dst);
void from_trap(const TrapFrame* src, InterruptFrame* dst);
void to_trap(const BlockOSSyscallFrame* src, TrapFrame* dst);
void from_trap(const TrapFrame* src, BlockOSSyscallFrame* dst);

}
