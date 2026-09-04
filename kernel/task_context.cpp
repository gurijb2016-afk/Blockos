#include "task_context.hpp"

namespace task
{

void init_context(
    TaskContext* ctx,
    uint64_t entry
)
{
    if (!ctx)
        return;

    ctx->rsp = 0;
    ctx->rip = entry;

    ctx->rbp = 0;

    ctx->rax = 0;
    ctx->rbx = 0;
    ctx->rcx = 0;
    ctx->rdx = 0;

    ctx->rsi = 0;
    ctx->rdi = 0;

    ctx->r8 = 0;
    ctx->r9 = 0;
    ctx->r10 = 0;
    ctx->r11 = 0;
    ctx->r12 = 0;
    ctx->r13 = 0;
    ctx->r14 = 0;
    ctx->r15 = 0;

    ctx->flags = 0x202;
}

void switch_to(
    TaskContext* ctx
)
{
    if (!ctx)
        return;

    if (ctx->rsp == 0 || ctx->rip == 0)
        return;

    asm volatile(
        "mov %0, %%rsp\n"
        "push %1\n"
        "ret\n"
        :
        : "r"(ctx->rsp),
          "r"(ctx->rip)
        : "memory"
    );
}

}
