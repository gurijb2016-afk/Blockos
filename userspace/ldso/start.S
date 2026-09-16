.text
.globl _start
.type _start,@function
_start:
    /*
     * On entry (per elf_loader::build_initial_stack in the kernel):
     *   rsp -> [argc][argv[0..argc-1]][NULL][envp[0..envc-1]][NULL][auxv pairs...][AT_NULL,0]
     * No return address was pushed (we got here via iretq, not call), and
     * rsp is 16-byte aligned as required by the SysV ABI at process entry.
     */
    xor %rbp, %rbp        /* mark the outermost frame for any unwinder */
    mov %rsp, %rdi         /* ld_main(void* raw_sp) */
    call ld_main
    /* ld_main() only returns on unrecoverable failure. */
    mov $44, %rax           /* SYS_exit */
    mov $1, %rdi
    int $0x80
.Lhang:
    hlt
    jmp .Lhang
.size _start,.-_start

/*
 * void ldso_jump_to_entry(void* original_sp, uint64_t entry) __attribute__((noreturn));
 * Restores the ORIGINAL stack pointer (untouched contents - argc/argv/envp/
 * auxv exactly as the kernel built them) and jumps to the real program's
 * entry point, exactly as if the kernel had loaded it directly.
 */
.globl ldso_jump_to_entry
.type ldso_jump_to_entry,@function
ldso_jump_to_entry:
    mov %rdi, %rsp
    xor %rbp, %rbp
    jmp *%rsi
.size ldso_jump_to_entry,.-ldso_jump_to_entry
