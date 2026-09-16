#include "preempt.hpp"
#include "process.hpp"
#include "arch/86_64x/paging.hpp"
#include <string.h>

namespace preempt {

void to_trap(const InterruptFrame* s, TrapFrame* d) {
    d->rax=s->rax; d->rbx=s->rbx; d->rcx=s->rcx; d->rdx=s->rdx;
    d->rsi=s->rsi; d->rdi=s->rdi; d->rbp=s->rbp;
    d->r8=s->r8; d->r9=s->r9; d->r10=s->r10; d->r11=s->r11;
    d->r12=s->r12; d->r13=s->r13; d->r14=s->r14; d->r15=s->r15;
    d->rip=s->rip; d->cs=s->cs; d->rflags=s->rflags; d->rsp=s->rsp; d->ss=s->ss;
}
void from_trap(const TrapFrame* s, InterruptFrame* d) {
    d->rax=s->rax; d->rbx=s->rbx; d->rcx=s->rcx; d->rdx=s->rdx;
    d->rsi=s->rsi; d->rdi=s->rdi; d->rbp=s->rbp;
    d->r8=s->r8; d->r9=s->r9; d->r10=s->r10; d->r11=s->r11;
    d->r12=s->r12; d->r13=s->r13; d->r14=s->r14; d->r15=s->r15;
    d->rip=s->rip; d->cs=s->cs; d->rflags=s->rflags; d->rsp=s->rsp; d->ss=s->ss;
    /* vector/error_code are left as-is: isr_common discards them with
     * `add $16,%rsp` before iretq, so their values never reach the CPU. */
}
void to_trap(const BlockOSSyscallFrame* s, TrapFrame* d) {
    memcpy(d, s, sizeof(TrapFrame)); /* identical layout */
}
void from_trap(const TrapFrame* s, BlockOSSyscallFrame* d) {
    memcpy(d, s, sizeof(TrapFrame));
}

namespace {

/*
 * Round-robin over process::Process slots. The scheduler deliberately
 * owns no storage of its own beyond this cursor - the saved frame lives
 * in the Process struct, so there is exactly one source of truth per task.
 */
static size_t rr_cursor = 0;

/*
 * Why kernel code is never preempted:
 *
 * The TSS has a single rsp0 (arch/86_64x/hardware_tables.cpp sets one
 * static `user_kernel_stack`). Every ring3 -> ring0 entry starts at the
 * top of that one stack. That is safe as long as at most ONE task is ever
 * inside the kernel at a time, which holds because we only ever switch
 * tasks at a point where the outgoing task's entire kernel-side state has
 * already been copied out into its Process::saved_frame - nothing of it
 * remains on the shared kernel stack.
 *
 * Preempting ring0 code would break that invariant: a half-finished
 * syscall's locals live on the shared kernel stack, and switching away
 * would let another task overwrite them. So we simply don't - a task that
 * is inside a syscall runs to completion, then gets preempted on a later
 * tick once it is back in ring3. Syscalls here are all short and
 * non-blocking, so this costs very little.
 */

static process::Process* pick_next(process::Process* skip) {
    size_t n = process::slot_count();
    for (size_t i = 0; i < n; i++) {
        size_t idx = (rr_cursor + 1 + i) % n;
        process::Process* p = process::slot_at(idx);
        if (!p || p == skip) continue;
        if (p->state != process::State::READY) continue;
        rr_cursor = idx;
        return p;
    }
    return nullptr;
}

/* Saves `from`'s state, activates `to`, and returns `to`'s frame to load. */
static void switch_to(process::Process* from, const TrapFrame* from_state,
                      process::Process* to, TrapFrame* out) {
    if (from && from->state == process::State::RUNNING) {
        from->saved_frame = *from_state;
        from->frame_valid = true;
        from->state = process::State::READY;
    }
    to->state = process::State::RUNNING;
    process::set_current(to);
    *out = to->saved_frame;
    paging::switch_pml4(to->pml4);
}

}

bool on_timer(InterruptFrame* f) {
    if (!f) return false;
    if ((f->cs & 3) != 3) return false;  /* kernel was running - see note above */

    process::Process* cur = process::current();
    process::Process* next = pick_next(cur);
    if (!next) return false;             /* nothing else runnable */

    TrapFrame cur_state, next_state;
    to_trap(f, &cur_state);
    switch_to(cur, &cur_state, next, &next_state);
    from_trap(&next_state, f);
    return true;
}

bool yield_from_syscall(BlockOSSyscallFrame* f) {
    if (!f) return false;
    process::Process* cur = process::current();
    process::Process* next = pick_next(cur);
    if (!next) return false;

    TrapFrame cur_state, next_state;
    to_trap(f, &cur_state);
    switch_to(cur, &cur_state, next, &next_state);
    from_trap(&next_state, f);
    return true;
}

bool on_exit(BlockOSSyscallFrame* f) {
    if (!f) return false;
    process::Process* cur = process::current();
    if (cur) {
        cur->state = process::State::TERMINATED;
        cur->frame_valid = false;
    }
    process::Process* next = pick_next(cur);
    if (!next) {
        process::set_current(nullptr);
        return false;                    /* caller returns into the kernel */
    }

    TrapFrame next_state;
    next->state = process::State::RUNNING;
    process::set_current(next);
    next_state = next->saved_frame;
    paging::switch_pml4(next->pml4);
    from_trap(&next_state, f);
    return true;
}

}
