.text
.globl __blockos_clone_and_start
.type __blockos_clone_and_start,@function
/* rdi=start_routine, rsi=arg, rdx=child_stack_top, rcx=tls, r8=ThreadStart record */
__blockos_clone_and_start:
    mov %rdi,%r12
    mov %rsi,%r13
    mov %rdx,%r14
    mov %rcx,%r15
    mov %r8,%rbx

    mov $42,%eax
    /* CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD */
    mov $0x00010F00,%edi
    mov %r14,%rsi
    mov %rbx,%rdx
    xor %r10d,%r10d
    mov %r15,%r8
    xor %r9d,%r9d
    int $0x80

    test %rax,%rax
    jnz .parent

    /* Kernel places the ThreadStart pointer in child RDI. */
    mov %rdi,%rbx
    mov 0(%rbx),%r12
    mov 8(%rbx),%r13
    mov %r13,%rdi
    call *%r12
    mov %rax,24(%rbx)
    movl $1,16(%rbx)

    /* FUTEX_WAKE(&record->done, 1) */
    lea 16(%rbx),%rdi
    mov $1,%esi
    mov $41,%eax
    mov $0xffffffff,%edx
    int $0x80

    xor %edi,%edi
    mov $45,%eax
    int $0x80
1:  hlt
    jmp 1b
.parent:
    ret
.size __blockos_clone_and_start,.-__blockos_clone_and_start
