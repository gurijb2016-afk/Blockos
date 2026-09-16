.text
.globl _start
.type _start,@function
_start:
    /*
     * Same raw stack layout as ld.so's start.S consumes:
     *   rsp -> [argc][argv[0..argc-1]][NULL][envp[0..]][NULL][auxv...]
     * Whether we got here directly from the kernel (static binary, no
     * PT_INTERP) or via ld.so jumping to us (dynamic binary), the layout
     * is identical - this crt1 doesn't need to know or care which.
     */
    xor %rbp, %rbp
    mov (%rsp), %rdi        /* argc */
    lea 8(%rsp), %rsi       /* argv */
    mov %rdi, %rax
    lea 16(%rsp,%rax,8), %rdx  /* envp = argv + (argc+1) */
    and $-16, %rsp          /* defensive re-alignment before the call */
    call main
    mov %eax, %edi
    call exit               /* never returns */
.Lhang:
    hlt
    jmp .Lhang
.size _start,.-_start
