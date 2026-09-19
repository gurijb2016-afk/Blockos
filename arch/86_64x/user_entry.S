.text
.globl blockos_user_enter
.globl blockos_user_return
.globl blockos_user_saved_rsp
.globl blockos_user_saved_cr3
.type blockos_user_enter,@function
blockos_user_enter:
    /* rdi=entry, rsi=stack, rdx=pml4, rcx=FS base */
    mov %rsp, blockos_user_saved_rsp(%rip)
    mov %cr3, %rax
    mov %rax, blockos_user_saved_cr3(%rip)
    mov %rdx, %cr3
    /* Activate the task-local TLS pointer before iretq. */
    mov %rcx, %rax
    mov %rax, %r8
    shr $32, %r8
    mov $0xC0000100, %ecx
    mov %r8d, %edx
    mov %eax, %eax
    wrmsr
    mov $0x23, %ax
    mov %ax,%ds
    mov %ax,%es
    mov %ax,%fs
    mov %ax,%gs
    pushq $0x23
    pushq %rsi
    pushq $0x202
    pushq $0x1b
    pushq %rdi
    iretq
.size blockos_user_enter,.-blockos_user_enter

.type blockos_user_return,@function
blockos_user_return:
    mov blockos_user_saved_cr3(%rip),%rax
    mov %rax,%cr3
    /* Leave FS base in a benign kernel value after a task exits. */
    xor %eax,%eax
    xor %edx,%edx
    mov $0xC0000100,%ecx
    wrmsr
    ret
.size blockos_user_return,.-blockos_user_return

.data
.align 8
blockos_user_saved_rsp: .quad 0
blockos_user_saved_cr3: .quad 0
