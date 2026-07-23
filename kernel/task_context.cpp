#include "task_context.hpp"
#include <stdint.h>


namespace task {


void init_context(
    TaskContext* ctx,
    uint64_t entry
)
{
    if (!ctx)
        return;


    ctx->rip = entry;

    // Kezdeti stack hely
    ctx->rsp = 0x00007FFFFFF00000;

    ctx->rbp = 0;

    ctx->rax = 0;
    ctx->rbx = 0;
    ctx->rcx = 0;
    ctx->rdx = 0;

    ctx->rsi = 0;
    ctx->rdi = 0;

    ctx->r8  = 0;
    ctx->r9  = 0;
    ctx->r10 = 0;
    ctx->r11 = 0;
    ctx->r12 = 0;
    ctx->r13 = 0;
    ctx->r14 = 0;
    ctx->r15 = 0;

    ctx->flags = 0x202;
}


void switch_to(TaskContext* ctx)
{
    /*
       Később ide jön:
       context_switch.S

       - regiszter mentés
       - CR3 váltás
       - új RIP/RSP betöltés
    */

    asm volatile(
        "mov %0, %%rsp\n"
        "jmp *%1\n"
        :
        : "r"(ctx->rsp),
          "r"(ctx->rip)
    );
}


}
