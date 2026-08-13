.global blockos_syscall_entry
.type blockos_syscall_entry,@function
blockos_syscall_entry:
    /* BlockOS-owned ABI:
       rax=syscall number, rdi/rsi/rdx/r10/r8/r9=args */
    push %r9
    push %r8
    push %r10
    push %rdx
    push %rsi
    push %rdi
    push %rax
    /* Real kernel entry/dispatch is architecture-specific. */
    pop %rax
    pop %rdi
    pop %rsi
    pop %rdx
    pop %r10
    pop %r8
    pop %r9
    ret
.size blockos_syscall_entry, .-blockos_syscall_entry
