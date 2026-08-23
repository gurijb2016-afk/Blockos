.text

.globl context_switch
.extern context_switch

context_switch:

    # --------------------------------------------------------
    # Save callee-saved registers
    # --------------------------------------------------------

    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    # --------------------------------------------------------
    # Save current stack pointer
    #
    # RDI -> uint64_t old_sp
    # --------------------------------------------------------

    movq %rsp, (%rdi)

    # --------------------------------------------------------
    # Switch address space if CR3 != 0
    #
    # RDX = new CR3
    # --------------------------------------------------------

    movq %rdx, %rax
    testq %rax, %rax
    jz .Lskip_cr3

    movq %rax, %cr3

.Lskip_cr3:

    # --------------------------------------------------------
    # Switch to new stack
    #
    # RSI = new stack pointer
    # --------------------------------------------------------

    movq %rsi, %rsp

    # --------------------------------------------------------
    # Restore callee-saved registers
    # --------------------------------------------------------

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp

    # --------------------------------------------------------
    # Return using the return address on the new stack
    # --------------------------------------------------------

    ret


# ============================================================
# task_start_trampoline
#
# Expected initial stack:
#
#   [RSP + 0]  = return/trampoline address
#   [RSP + 8]  = task entry function
#   [RSP + 16] = task argument
#
# After context_switch:
#
#   pop %rdi -> entry
#   pop %rsi -> argument
#
# Then:
#   RAX = entry
#   RDI = argument
#   call *RAX
#   call task_exit
# ============================================================

.globl task_start_trampoline
.extern task_start_trampoline
.extern task_exit

task_start_trampoline:

    # --------------------------------------------------------
    # Get task entry address
    # --------------------------------------------------------

    popq %rdi

    # --------------------------------------------------------
    # Get task argument
    # --------------------------------------------------------

    popq %rsi

    # --------------------------------------------------------
    # Save entry address in RAX
    # --------------------------------------------------------

    movq %rdi, %rax

    # --------------------------------------------------------
    # First argument of entry(void*) = RDI
    # --------------------------------------------------------

    movq %rsi, %rdi

    # --------------------------------------------------------
    # Call task entry
    # --------------------------------------------------------

    call *%rax

    # --------------------------------------------------------
    # Task returned -> terminate task
    # --------------------------------------------------------

    call task_exit

.Ltask_exit_halt:

    cli
    hlt
    jmp .Ltask_exit_halt
