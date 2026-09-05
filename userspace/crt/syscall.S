.text
.globl blockos_syscall6
.type blockos_syscall6,@function
blockos_syscall6:
    push %rbp
    mov %rsp,%rbp
    mov %rdi,%rax
    mov %rsi,%rdi
    mov %rdx,%rsi
    mov %rcx,%rdx
    mov %r8,%r10
    mov %r9,%r8
    mov 16(%rbp),%r9
    int $0x80
    pop %rbp
    ret
.size blockos_syscall6,.-blockos_syscall6
