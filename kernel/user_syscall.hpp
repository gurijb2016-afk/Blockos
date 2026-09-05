#pragma once
#include <stdint.h>
struct BlockOSSyscallFrame{uint64_t rax,rbx,rcx,rdx,rsi,rdi,rbp,r8,r9,r10,r11,r12,r13,r14,r15,rip,cs,rflags,rsp,ss;};
extern "C" void blockos_syscall_dispatch_frame(BlockOSSyscallFrame*);
