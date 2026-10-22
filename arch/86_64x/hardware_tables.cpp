#include "hardware_tables.hpp"
#include "irq.hpp"
#include "isr_stubs.hpp"
#include "libc/include/stdio.h"
#include "libc/include/string.h"
#include "preempt.hpp"
extern "C" void reload_segments();
extern "C" void blockos_syscall_entry();
__attribute__((aligned(4096))) static GDTEntry gdt[7]; static GDTDescriptor gdt_desc; __attribute__((aligned(16))) static IDTEntry idt[256]; __attribute__((aligned(16))) static TSS tss; __attribute__((aligned(16))) static uint8_t df_stack[8192]; __attribute__((aligned(16))) static uint8_t user_kernel_stack[16384];
HardwareTablesManager cpu_tables;
void HardwareTablesManager::register_stubs(){for(int i=0;i<256;i++)register_interrupt_handler(i,(void*)default_stubs[i],0x8E);register_interrupt_handler(128,(void*)blockos_syscall_entry,0xEE);}
void HardwareTablesManager::init(){setup_cpu_security();register_stubs();idt[8].ist=1;load_idt();pic_remap();pit_init(100);__asm__ volatile("sti");}
void HardwareTablesManager::setup_cpu_security(){__asm__ volatile("cli");gdt[0]={0,0,0,0,0,0};gdt[1]={0,0,0,0x9A,0x20,0};gdt[2]={0,0,0,0x92,0,0};gdt[3]={0,0,0,0xFA,0x20,0};gdt[4]={0,0,0,0xF2,0,0};setup_tss();gdt_desc.limit=sizeof(gdt)-1;gdt_desc.base=(uint64_t)&gdt;__asm__ volatile("lgdt %0"::"m"(gdt_desc));reload_segments();__asm__ volatile("ltr %w0"::"r"((uint16_t)0x28));}
void HardwareTablesManager::setup_tss(){memset(&tss,0,sizeof(tss));tss.rsp0=(uint64_t)user_kernel_stack+sizeof(user_kernel_stack);tss.ist1=(uint64_t)df_stack+sizeof(df_stack);tss.iomap_base=sizeof(tss);uint64_t b=(uint64_t)&tss;uint32_t l=sizeof(tss)-1;gdt[5]={(uint16_t)(l&0xffff),(uint16_t)(b&0xffff),(uint8_t)((b>>16)&0xff),0x89,(uint8_t)((l>>16)&0x0f),(uint8_t)((b>>24)&0xff)};gdt[6]={0,(uint16_t)((b>>32)&0xffff),(uint8_t)((b>>48)&0xff),0,0,(uint8_t)((b>>56)&0xff)};}

/*
 * InterruptFrame now lives in preempt.hpp so the scheduler and this
 * handler cannot drift apart - a mismatch between the two would corrupt
 * every saved register on a task switch, which is exactly the kind of bug
 * that is near-impossible to find after the fact.
 */
extern "C" void blockos_panic_flush(void);
/* Last CPU exception, so the boot path can report why a process ended. */
extern "C" {
volatile uint64_t blockos_fault_vector = 0;
volatile uint64_t blockos_fault_error = 0;
volatile uint64_t blockos_fault_rip = 0;
volatile uint64_t blockos_fault_cr2 = 0;
}
extern "C" void blockos_user_return();
extern "C" uint64_t blockos_user_saved_rsp;

static const char* exception_name(uint64_t v){
    switch(v){
        case 0: return "divide error";
        case 6: return "invalid opcode";
        case 8: return "double fault";
        case 13: return "general protection";
        case 14: return "page fault";
        default: return "CPU exception";
    }
}

extern "C" void interrupt_handler_c(InterruptFrame* f){
    if(f->vector<32){
        uint64_t cr2 = 0;
        if(f->vector==14) __asm__ volatile("mov %%cr2,%0":"=r"(cr2));
        const bool from_user = (f->cs & 3) == 3;
        blockos_fault_vector = f->vector;
        blockos_fault_error = f->error_code;
        blockos_fault_rip = f->rip;
        blockos_fault_cr2 = cr2;
        printf("\n*** BlockOS %s ***\n", from_user ? "PROCESS FAULT" : "KERNEL PANIC");
        printf("%s (vector %llu) error=0x%llx rip=0x%llx",
               exception_name(f->vector), f->vector, f->error_code, f->rip);
        if(f->vector==14) printf(" cr2=0x%llx", cr2);
        printf("\n");
        if(!from_user){
            printf("CPU halted.\n");
            blockos_panic_flush();
            for(;;)__asm__ volatile("cli;hlt");
        }
        /* User-mode fault: end the process through the same return path as
         * exit(), then report it. The kernel's caller then stops the system. */
        printf("process terminated.\n");
        blockos_panic_flush();
        f->rip = (uint64_t)(uintptr_t)&blockos_user_return;
        f->cs = 0x08;
        f->ss = 0x10;
        f->rsp = blockos_user_saved_rsp;
        f->rflags = 0x202;
        return;
    }
    if(f->vector<48){
        if(f->vector==32) pit_handler_c();
        pic_send_eoi((uint8_t)f->vector);

        /* EOI is sent BEFORE switching, so the PIC is ready for the next
         * tick regardless of which task we return into. on_timer() only
         * acts when the interrupted code was ring3 and another task is
         * READY; otherwise the frame is untouched and isr_common's iretq
         * resumes exactly what was running. */
        if(f->vector==32) preempt::on_timer(f);
    }
}

void HardwareTablesManager::register_interrupt_handler(uint8_t v,void* h,uint8_t flags){uint64_t a=(uint64_t)h;idt[v].isr_low=a&0xffff;idt[v].cs_selector=0x08;idt[v].ist=0;idt[v].attributes=flags;idt[v].isr_mid=(a>>16)&0xffff;idt[v].isr_high=(a>>32)&0xffffffff;idt[v].reserved=0;}
void HardwareTablesManager::load_idt(){IDTR x={sizeof(idt)-1,(uint64_t)&idt};asm volatile("lidt %0"::"m"(x));}
