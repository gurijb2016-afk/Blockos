    .text
    .global context_switch
// context_switch(old_sp_ptr, new_sp, new_cr3)
// old_sp_ptr: pointer to uint64_t where current rsp will be stored
// new_sp: value of rsp to load
// new_cr3: new CR3 value to load before switching stacks
context_switch:
    // Save callee-saved registers onto stack
    push %rbp
    push %rbx
    push %r12
    push %r13
    push %r14
    push %r15

    // Store current rsp into *old_sp_ptr (first arg in rdi)
    mov %rsp, (%rdi)

    // Load new CR3 from rdx
    mov %rdx, %cr3

    // Load new stack pointer from rsi
    mov %rsi, %rsp

    // Restore callee-saved from new stack in reverse order
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rbx
    pop %rbp

    ret


    .global task_start_trampoline
// Trampoline that starts a task. Stack layout expected (from low->high addresses):
// [..callee-saved slots..][return_addr -> task_start_trampoline][entry_addr][arg]
// After context_switch returns, ret will jump here, and the stack will point to entry_addr.
// We pop entry_addr into rax, arg into rdi, then call rax(entry_arg). On return, call task_exit.
task_start_trampoline:
    // Pop entry pointer and argument
    pop %rdi    // entry_addr -> rdi
    pop %rsi    // arg -> rsi
    mov %rdi, %rax  // rax = entry_addr
    mov %rsi, %rdi  // first arg in rdi
    call *%rax
    // On return, call task_exit (C function)
    extern task_exit
    call task_exit
    // Should not return
    hlt
