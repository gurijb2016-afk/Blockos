.text
.globl _start
.type _start,@function
_start:
    xor %rbp, %rbp
    mov %rsp, %rdi
    and $-16, %rsp
    call ld_main
    mov $44, %rax
    mov $127, %rdi
    int $0x80
1:
    hlt
    jmp 1b
.size _start,.-_start

.globl ldso_jump_to_entry
.type ldso_jump_to_entry,@function
ldso_jump_to_entry:
    mov %rdi, %rsp
    xor %rbp, %rbp
    jmp *%rsi
.size ldso_jump_to_entry,.-ldso_jump_to_entry
