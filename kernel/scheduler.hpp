#pragma once

#include <stdint.h>
#include <stddef.h>

namespace scheduler
{

enum class TaskState : uint8_t
{
    Empty = 0,
    Ready,
    Running,
    Sleeping,
    Blocked,
    Finished
};

struct Task
{
    uint64_t id;

    uint32_t priority;
    uint32_t cpu;

    uint64_t runtime_ns;
    uint64_t deadline_ns;

    TaskState state;

    void (*entry)(void*);
    void* arg;
};

struct Decision
{
    uint64_t task_id;
    bool run;
    bool preempt_current;
};

struct Policy
{
    const char* name;

    bool (*init)();
    void (*destroy)();

    bool (*enqueue)(
        Task* task
    );

    void (*dequeue)(
        Task* task
    );

    Task* (*pick_next)(
        Task* current
    );

    void (*task_running)(
        Task* task
    );

    void (*task_blocked)(
        Task* task
    );

    void (*task_finished)(
        Task* task
    );

    bool (*should_preempt)(
        Task* current,
        Task* candidate
    );
};

void scheduler_init();

int create_task(
    void (*entry)(void*),
    void* arg
);

bool destroy_task(
    uint64_t task_id
);

Task* get_task(
    uint64_t task_id
);

Task* current_task();

size_t task_count();

void yield();

void run_scheduler_loop();

bool register_policy(
    const Policy* policy
);

bool unregister_policy(
    const char* name
);

bool select_policy(
    const char* name
);

const Policy* current_policy();

const char* current_policy_name();

Task* pick_next();

Decision make_decision();

void scheduler_tick(
    uint64_t elapsed_ns
);

bool set_ready(
    uint64_t task_id
);

bool set_running(
    uint64_t task_id
);

bool set_sleeping(
    uint64_t task_id
);

bool set_blocked(
    uint64_t task_id
);

bool set_finished(
    uint64_t task_id
);

}
