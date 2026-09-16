#include "process.hpp"
#include "elf_loader.hpp"
#include "arch/86_64x/paging.hpp"
#include "vfs.hpp"
#include <string.h>

extern "C" void blockos_user_enter(uint64_t entry, uint64_t stack, uint64_t pml4);
extern "C" uint64_t blockos_user_saved_rsp;
extern "C" void blockos_user_return();

namespace process {

namespace {
constexpr uint64_t PAGE = 0x1000;
constexpr uint64_t STACK_TOP = 0x00007ffffff00000ULL;
constexpr uint64_t STACK_PAGES = 32;
constexpr uint64_t DEFAULT_INTERP_BASE = 0x0000700000000000ULL;
constexpr uint64_t USER_FLAGS = 4 | 2 | (1ULL << 63);

/* Selectors set up by hardware_tables.cpp: gdt[3]=user code, gdt[4]=user
 * data, both with RPL 3 -> 0x1b / 0x23. These match what user_entry.S
 * pushes for its iretq, so a task entered by the scheduler and a task
 * entered by blockos_user_enter start in exactly the same CPU state. */
constexpr uint64_t USER_CS = 0x1b;
constexpr uint64_t USER_SS = 0x23;
constexpr uint64_t USER_RFLAGS = 0x202;   /* IF set, everything else default */
}

static constexpr size_t MAX_PROCESS = 128;
static Process ps[MAX_PROCESS];
static uint64_t next_pid = 1;
static Process* cur = nullptr;

static Process* free_slot() { for (auto& p : ps) if (p.state == State::EMPTY) return &p; return nullptr; }

void init() { memset(ps, 0, sizeof(ps)); for (auto& p : ps) p.state = State::EMPTY; next_pid = 1; cur = nullptr; }

size_t slot_count() { return MAX_PROCESS; }
Process* slot_at(size_t i) { return i < MAX_PROCESS ? &ps[i] : nullptr; }
void set_current(Process* p) { cur = p; }

Process* spawn(const void* elf, size_t size, const char* path) {
    Process* p = free_slot();
    if (!p || !elf || !size) return nullptr;

    uint64_t pml4 = paging::clone_current_pml4();
    if (!pml4) return nullptr;

    elf_loader::LoadResult main_image;
    if (!elf_loader::load_elf64_into(pml4, elf, size, 0, &main_image)) return nullptr;

    memset(&p->context, 0, sizeof(p->context));
    memset(&p->saved_frame, 0, sizeof(p->saved_frame));
    p->pid = next_pid++;
    p->pml4 = pml4;
    p->task_id = 0;
    p->brk_base = 0x0000000200000000ULL;
    p->brk_current = p->brk_base;
    p->mmap_next = 0x0000000100000000ULL;
    p->has_interp = main_image.has_interp;
    p->real_entry = 0;

    uint64_t start_entry, start_rsp;

    if (!main_image.has_interp) {
        uint64_t stack_base = STACK_TOP - STACK_PAGES * PAGE;
        if (!paging::map_user_range(pml4, stack_base, STACK_PAGES * PAGE, USER_FLAGS))
            return nullptr;
        start_entry = main_image.entry;
        start_rsp = STACK_TOP - 16;
    } else {
        uint32_t interp_size = 0;
        const uint8_t* interp_buf = vfs::read_file(main_image.interp_path, &interp_size);
        if (!interp_buf || interp_size == 0) return nullptr;

        elf_loader::LoadResult interp_image;
        if (!elf_loader::load_elf64_into(pml4, interp_buf, interp_size, DEFAULT_INTERP_BASE, &interp_image))
            return nullptr;
        if (interp_image.has_interp) return nullptr;

        const char* argv[1] = { path ? path : "/bin/app" };
        const char* envp[1] = { nullptr };
        uint64_t rsp = elf_loader::build_initial_stack(
            pml4, STACK_TOP, STACK_PAGES, argv, 1, envp, 0,
            main_image, interp_image.load_bias);
        if (!rsp) return nullptr;

        start_entry = interp_image.entry;
        p->real_entry = main_image.entry;
        start_rsp = rsp;
    }

    p->entry = start_entry;
    p->stack = start_rsp;

    /* Seed the frame as if the task had just been preempted at its very
     * first instruction. This is what lets the scheduler treat a brand-new
     * task and a long-running one identically. */
    p->saved_frame.rip    = start_entry;
    p->saved_frame.rsp    = start_rsp;
    p->saved_frame.cs     = USER_CS;
    p->saved_frame.ss     = USER_SS;
    p->saved_frame.rflags = USER_RFLAGS;
    p->frame_valid = true;

    p->state = State::READY;
    return p;
}

Process* create(const void* elf, size_t size, const char* path) {
    return spawn(elf, size, path);
}

int run(Process* p) {
    if (!p || p->state != State::READY) return -1;
    cur = p;
    p->state = State::RUNNING;
    /* Enters ring3. Returns here only once NO task is runnable any more:
     * the exit path (preempt::on_exit) switches directly into the next
     * task when there is one, and only falls back to returning into the
     * kernel when the last task has exited. */
    blockos_user_enter(p->entry, p->stack, p->pml4);
    if (p->state == State::RUNNING) p->state = State::TERMINATED;
    cur = nullptr;
    return 0;
}

void run_scheduler() {
    for (auto& p : ps) {
        if (p.state == State::READY) { run(&p); return; }
    }
}

bool terminate(Process* p) {
    if (!p || p->state == State::EMPTY) return false;
    p->state = State::TERMINATED;
    if (cur == p) cur = nullptr;
    return true;
}

Process* current() { return cur; }
Process* get(uint64_t pid) { if (!pid) return nullptr; for (auto& p : ps) if (p.state != State::EMPTY && p.pid == pid) return &p; return nullptr; }
size_t count() { size_t n = 0; for (auto& p : ps) if (p.state != State::EMPTY) n++; return n; }

}
