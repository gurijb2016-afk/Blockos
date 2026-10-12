/* Switch an EFI MP Services AP from the firmware stack to a BlockOS-owned stack.
 * SysV ABI on entry: rdi = logical CPU index, rsi = stack top.
 * The function never returns to the EFI callback. */
.text
.globl blockos_smp_ap_stack_switch
.type blockos_smp_ap_stack_switch,@function
blockos_smp_ap_stack_switch:
    mov %rsi, %rsp
    andq $-16, %rsp
    xor %rbp, %rbp
    call blockos_smp_ap_main
1:
    cli
    hlt
    jmp 1b
.size blockos_smp_ap_stack_switch,.-blockos_smp_ap_stack_switch
.section .note.GNU-stack,"",@progbits
