#pragma once
#include <stdint.h>
#include <stddef.h>
#include "task_context.hpp"
#include "preempt.hpp"

namespace process {

enum class State : uint8_t { EMPTY = 0, READY, RUNNING, BLOCKED, TERMINATED };

struct RuntimeFd {
    enum Kind : uint8_t { None = 0, Tty, File, Directory, Device, UnixSocket };
    bool used;
    Kind kind;
    uint16_t flags;
    uint32_t object;
    const uint8_t* data;
    uint64_t size;
    uint64_t off;
};

constexpr size_t MAX_RUNTIME_FDS = 128;

struct Process {
    uint64_t pid, pml4, entry, stack, task_id, brk_base, brk_current, mmap_next;
    State state;
    TaskContext context;
    bool has_interp;
    uint64_t real_entry;

    /* POSIX/Linux-style thread/runtime state. Threads share their leader's
     * address space and file descriptor table but have independent FS bases
     * and saved ring3 frames. */
    Process* fd_owner;
    uint64_t parent_pid;
    uint64_t tid;
    uint64_t fs_base;
    uint64_t wake_deadline_ms;
    uint64_t exit_code;
    uint64_t clear_tid;
    bool is_thread;
    char name[64];
    char cwd[256];
    RuntimeFd fds[MAX_RUNTIME_FDS];

    /* Minimal per-thread signal state used by GLib/GIO/GNOME's POSIX
     * startup code.  It intentionally keeps the ABI small: handlers are
     * installed and masks are tracked, while actual asynchronous delivery
     * remains a later kernel feature. */
    uint64_t signal_mask;
    uint64_t signal_handlers[64];
    uint64_t signal_flags[64];

    /* Full ring3 state while this task is not the running one. Seeded at
     * creation time so a never-yet-run task can be switched into exactly
     * like one that was preempted mid-execution - the scheduler needs no
     * special "first run" case. */
    TrapFrame saved_frame;
    bool frame_valid;
};

void init();

/* Creates a process in READY state (loads the ELF, sets up its address
 * space and initial frame) but does NOT start it. */
Process* spawn(const void* elf, size_t size, const char* path = nullptr);

/* Backwards-compatible: spawn + run immediately, blocking until it (and
 * everything else runnable) has finished. Existing kernel.cpp calls keep
 * working unchanged. */
Process* create(const void* elf, size_t size, const char* path = nullptr);
int run(Process* proc);

/* Starts the scheduler: enters the first READY task in ring3 and returns
 * only once every task has exited. */
void run_scheduler();

bool terminate(Process* proc);
Process* current();
RuntimeFd* fds(Process* p);
void set_current(Process* p);
Process* get(uint64_t pid);
size_t count();
const char* cwd(Process* p);
bool set_cwd(Process* p, const char* path);

/* Raw slot access for the scheduler's round-robin walk. slot_at() may
 * return a slot in any state, including EMPTY. */
size_t slot_count();
Process* slot_at(size_t i);

}
