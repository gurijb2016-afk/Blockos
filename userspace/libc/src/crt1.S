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
    /* Static binaries get no TLS from ld.so; errno lives in TLS, so set up
     * FS first. A no-op when FS is already set (dynamic binaries). rbx keeps
     * the original stack pointer across the call. */
    mov %rsp, %rbx
    and $-16, %rsp
    call __blockos_static_tls_setup
    mov %rbx, %rsp
    mov (%rsp), %r12                 /* argc (kept in callee-saved r12) */
    lea 8(%rsp), %r13                /* argv */
    lea 16(%rsp,%r12,8), %r14        /* envp = argv + (argc+1) */
    and $-16, %rsp                   /* ABI alignment before the calls */
    mov %r14, %rdi                   /* __blockos_set_environ(envp) */
    call __blockos_set_environ
    /* This toolchain's crtbegin registers .eh_frame from .init, which crt1
     * never runs, so register it here when libgcc_eh is linked (weak for pure C). */
    .weak __register_frame_info
    lea __register_frame_info(%rip), %rax
    test %rax, %rax
    jz .Lskip_eh_reg
    lea __blockos_eh_frame_begin(%rip), %rdi
    lea __blockos_eh_object(%rip), %rsi
    call *%rax
.Lskip_eh_reg:
    /* Run C++ static constructors (.init_array) before main(). */
    lea __init_array_start(%rip), %r15
    lea __init_array_end(%rip), %rbx
1:  cmp %rbx, %r15
    jae 2f
    call *(%r15)
    add $8, %r15
    jmp 1b
2:
    mov %r12, %rdi                   /* main(argc, argv, envp) */
    mov %r13, %rsi
    mov %r14, %rdx
    call main
    mov %eax, %edi
    call exit               /* never returns */
.Lhang:
    hlt
    jmp .Lhang
.size _start,.-_start

.section .eh_frame,"a",@progbits
.globl __blockos_eh_frame_begin
__blockos_eh_frame_begin:
.section .bss
.balign 16
__blockos_eh_object:
    .zero 128
.text
