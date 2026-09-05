.text
.globl _start
.type _start,@function
_start:
    xor %rbp,%rbp
    mov (%rsp),%rdi
    lea 8(%rsp),%rsi
    xor %rdx,%rdx
    call main
    mov %rax,%rdi
    mov $44,%rax
    int $0x80
1:  hlt
    jmp 1b
.size _start,.-_start
