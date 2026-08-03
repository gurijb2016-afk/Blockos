.text

# ============================================================
# context_switch
#
# Arguments - System V AMD64 ABI:
#
#   RDI = uint64_t* old_sp_ptr
#   RSI = uint64_t  new_sp
#   RDX = uint64_t  new_cr3
#
# ============================================================

.global context_switch
.type context_switch, @function

context_switch:

    # --------------------------------------------------------
    # Save callee-saved registers
    # --------------------------------------------------------

    push %rbp
    push %rbx
    push %r12
    push %r13
    push %r14
    push %r15

    # --------------------------------------------------------
    # Save current RSP
    # --------------------------------------------------------

    mov %rsp, (%rdi)

    # --------------------------------------------------------
    # Switch address space
    # --------------------------------------------------------

    mov %rdx, %rax

    test %rax, %rax
    jz 1f

    mov %rax, %cr3

1:

    # --------------------------------------------------------
    # Switch stack
    # --------------------------------------------------------

    mov %rsi, %rsp

    # --------------------------------------------------------
    # Restore callee-saved registers
    # --------------------------------------------------------

    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rbx
    pop %rbp

    # --------------------------------------------------------
    # Return to saved instruction/trampoline
    # --------------------------------------------------------

    ret


# ============================================================
# task_start_trampoline
#
# Initial task stack:
#
#   rbp
#   rbx
#   r12
#   r13
#   r14
#   r15
#   return address -> task_start_trampoline
#   entry
#   arg
#
# context_switch restores the six registers and RET jumps
# here.
# ============================================================

.global task_start_trampoline
.type task_start_trampoline, @function

.extern task_exit

task_start_trampoline:

    # --------------------------------------------------------
    # Get task entry address
    # --------------------------------------------------------

    pop %rdi

    # --------------------------------------------------------
    # Get task argument
    # --------------------------------------------------------

    pop %rsi

    # --------------------------------------------------------
    # entry function -> RAX
    # --------------------------------------------------------

    mov %rdi, %rax

    # --------------------------------------------------------
    # First argument of entry(void*) -> RDI
    # --------------------------------------------------------

    mov %rsi, %rdi

    # --------------------------------------------------------
    # Call task entry
    # --------------------------------------------------------

    call *%rax

    # --------------------------------------------------------
    # Task returned -> terminate task
    # --------------------------------------------------------

    call task_exit

# task_exit should not return.
# Protect against accidental return.

.Ltask_exit_halt:

    cli
    hlt
    jmp .Ltask_exit_halt


# ============================================================
# GNU stack marker
#
# Prevents executable-stack warning.
# ============================================================

.section .note.GNU-stack,"",@progbits