.macro ISR_NOERR num
.global isr_stub_\num

isr_stub_\num:
    push $0
    push $\num
    jmp isr_common
.endm

.macro ISR_ERR num
.global isr_stub_\num

isr_stub_\num:
    push $\num
    jmp isr_common
.endm

ISR_NOERR 0    // #DE
ISR_NOERR 1    // #DB
ISR_NOERR 2    // NMI
ISR_NOERR 3    // #BP
ISR_NOERR 4    // #OF
ISR_NOERR 5    // #BR
ISR_NOERR 6    // #UD
ISR_NOERR 7    // #NM
ISR_ERR   8    // #DF
ISR_NOERR 9    // reserved
ISR_ERR   10   // #TS
ISR_ERR   11   // #NP
ISR_ERR   12   // #SS
ISR_ERR   13   // #GP
ISR_ERR   14   // #PF
ISR_NOERR 15   // reserved
ISR_NOERR 16   // #MF
ISR_ERR   17   // #AC
ISR_NOERR 18   // #MC
ISR_NOERR 19   // #XM
ISR_NOERR 20   // #VE
ISR_ERR   21   // #CP
ISR_NOERR 22   // reserved
ISR_NOERR 23   // reserved
ISR_NOERR 24   // reserved
ISR_NOERR 25   // reserved
ISR_NOERR 26   // reserved
ISR_NOERR 27   // reserved
ISR_NOERR 28   // #HV
ISR_ERR   29   // #VC
ISR_ERR   30   // #SC
ISR_NOERR 31   // reserved
ISR_NOERR 32 // timer

// Generate default stubs for every vector from 33 to 255
.irp num, 33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50 ,\
           51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68 ,\
           69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86 ,\
           87,88,89,90,91,92,93,94,95,96,97,98,99,100,101,102,103,104 ,\
           105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122 ,\
           123,124,125,126,127,128,129,130,131,132,133,134,135,136,137,138,139,140 ,\
           141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158 ,\
           159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176 ,\
           177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194 ,\
           195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212 ,\
           213,214,215,216,217,218,219,220,221,222,223,224,225,226,227,228,229,230 ,\
           231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248 ,\
           249,250,251,252,253,254,255
    ISR_NOERR \num
.endr

.text

.global isr_common
isr_common:
    push %r15
    push %r14
    push %r13
    push %r12
    push %r11
    push %r10
    push %r9
    push %r8
    push %rbp
    push %rdi
    push %rsi
    push %rdx
    push %rcx
    push %rbx
    push %rax

    mov %rsp, %rdi
    call interrupt_handler_c

     // Restore in reverse order
    pop %rax
    pop %rbx
    pop %rcx
    pop %rdx
    pop %rsi
    pop %rdi
    pop %rbp
    pop %r8
    pop %r9
    pop %r10
    pop %r11
    pop %r12
    pop %r13
    pop %r14
    pop %r15

    // Discard vector_num and error_code the stub pushed
    add $16, %rsp

    iretq


.global reload_segments
reload_segments:
    lea reload_target(%rip), %rax
    push $0x08
    push %rax
    lretq

reload_target:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov %ax, %fs
    mov %ax, %gs
    ret