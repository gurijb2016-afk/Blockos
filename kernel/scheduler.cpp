#include "scheduler.hpp"

extern "C" {
#include <efi.h>
}
extern "C" {
#include <efilib.h>
}

#include <string.h>
#include <stdint.h>
#include <stddef.h>

namespace {

constexpr size_t MAX_TASKS = 256;
constexpr size_t MAX_POLICIES = 16;

scheduler::Task tasks[MAX_TASKS];

const scheduler::Policy*
policies[MAX_POLICIES];

size_t policy_count = 0;

const scheduler::Policy*
active_policy = nullptr;

scheduler::Task*
current = nullptr;

uint64_t next_task_id = 1;

uint64_t scheduler_time_ns = 0;

// ============================================================
// Helpers
// ============================================================

static void print_policy(
    const char* name
) {
    if (!name)
        return;

    CHAR16 buffer[128];

    const CHAR16 fmt[] = {
        '[','s','c','h','e','d',']',' ',
        'p','o','l','i','c','y',':',' ',
        '%','s',
        '\n',
        0
    };

    UnicodeSPrint(
        buffer,
        sizeof(buffer),
        fmt,
        name
    );

    Print(buffer);
}

static scheduler::Task*
find_free_task() {
    for (size_t i = 0; i < MAX_TASKS; ++i) {
        if (tasks[i].state ==
            scheduler::TaskState::Empty) {
            return &tasks[i];
        }
    }

    return nullptr;
}

static scheduler::Task*
find_task(
    uint64_t id
) {
    if (id == 0)
        return nullptr;

    for (size_t i = 0; i < MAX_TASKS; ++i) {
        if (tasks[i].state !=
            scheduler::TaskState::Empty &&
            tasks[i].id == id) {
            return &tasks[i];
        }
    }

    return nullptr;
}

// ============================================================
// Default scheduler policy
//
// Priority + aging.
// ============================================================

static bool default_init() {
    return true;
}

static void default_destroy() {
}

static bool default_enqueue(
    scheduler::Task*
) {
    return true;
}

static void default_dequeue(
    scheduler::Task*
) {
}

static scheduler::Task*
default_pick_next(
    scheduler::Task* current_task
) {
    scheduler::Task* best = nullptr;

    uint64_t best_score = 0;

    for (size_t i = 0; i < MAX_TASKS; ++i) {
        scheduler::Task& task = tasks[i];

        if (task.state !=
            scheduler::TaskState::Ready) {
            continue;
        }

        /*
         * Higher priority wins.
         * Older runtime gets a small aging bonus.
         */
        uint64_t score =
            static_cast<uint64_t>(task.priority) * 1000ULL;

        score += task.runtime_ns / 1000000ULL;

        /*
         * Avoid immediately returning the same task
         * when another task is ready.
         */
        if (current_task &&
            task.id == current_task->id &&
            best != nullptr) {
            score /= 2;
        }

        if (!best || score > best_score) {
            best = &task;
            best_score = score;
        }
    }

    return best;
}

static void default_running(
    scheduler::Task*
) {
}

static void default_blocked(
    scheduler::Task*
) {
}

static void default_finished(
    scheduler::Task*
) {
}

static bool default_should_preempt(
    scheduler::Task* current_task,
    scheduler::Task* candidate
) {
    if (!candidate)
        return false;

    if (!current_task)
        return true;

    if (candidate->priority >
        current_task->priority)
        return true;

    return false;
}

static const scheduler::Policy default_policy = {
    "blockos-default",

    default_init,
    default_destroy,

    default_enqueue,
    default_dequeue,

    default_pick_next,

    default_running,
    default_blocked,
    default_finished,

    default_should_preempt
};

} // namespace

namespace scheduler {

// ============================================================
// Initialization
// ============================================================

void scheduler_init()
{
    memset(
        tasks,
        0,
        sizeof(tasks)
    );

    memset(
        policies,
        0,
        sizeof(policies)
    );

    policy_count = 0;
    active_policy = nullptr;
    current = nullptr;

    next_task_id = 1;
    scheduler_time_ns = 0;

    /*
     * Register built-in policy.
     */
    register_policy(
        &default_policy
    );

    select_policy(
        "blockos-default"
    );
}

// ============================================================
// Task creation
// ============================================================

int create_task(
    void (*entry)(void*),
    void* arg
) {
    if (!entry)
        return -1;

    Task* task = find_free_task();

    if (!task)
        return -1;

    memset(
        task,
        0,
        sizeof(Task)
    );

    task->id =
        next_task_id++;

    task->priority = 10;
    task->cpu = 0;

    task->runtime_ns = 0;
    task->deadline_ns = 0;

    task->state =
        TaskState::Ready;

    task->entry = entry;
    task->arg = arg;

    if (active_policy &&
        active_policy->enqueue) {

        if (!active_policy->enqueue(task)) {
            task->state =
                TaskState::Empty;

            return -1;
        }
    }

    return static_cast<int>(
        task->id
    );
}

// ============================================================
// Destroy
// ============================================================

bool destroy_task(
    uint64_t task_id
) {
    Task* task =
        find_task(task_id);

    if (!task)
        return false;

    if (active_policy &&
        active_policy->dequeue) {

        active_policy->dequeue(task);
    }

    if (current == task)
        current = nullptr;

    memset(
        task,
        0,
        sizeof(Task)
    );

    task->state =
        TaskState::Empty;

    return true;
}

// ============================================================
// Lookup
// ============================================================

Task* get_task(
    uint64_t task_id
) {
    return find_task(task_id);
}

Task* current_task() {
    return current;
}

size_t task_count() {
    size_t count = 0;

    for (size_t i = 0; i < MAX_TASKS; ++i) {
        if (tasks[i].state !=
            TaskState::Empty) {

            ++count;
        }
    }

    return count;
}

// ============================================================
// Policy registration
// ============================================================

bool register_policy(
    const Policy* policy
) {
    if (!policy ||
        !policy->name ||
        policy->name[0] == '\0') {

        return false;
    }

    if (policy_count >= MAX_POLICIES)
        return false;

    /*
     * Duplicate name protection.
     */
    for (size_t i = 0; i < policy_count; ++i) {
        if (policies[i] &&
            strcmp(
                policies[i]->name,
                policy->name
            ) == 0) {

            return false;
        }
    }

    if (policy->init) {
        if (!policy->init())
            return false;
    }

    policies[policy_count++] =
        policy;

    return true;
}

// ============================================================
// Policy removal
// ============================================================

bool unregister_policy(
    const char* name
) {
    if (!name)
        return false;

    for (size_t i = 0; i < policy_count; ++i) {

        if (!policies[i])
            continue;

        if (strcmp(
                policies[i]->name,
                name
            ) != 0) {

            continue;
        }

        /*
         * Never remove the active policy.
         */
        if (active_policy ==
            policies[i]) {

            return false;
        }

        if (policies[i]->destroy)
            policies[i]->destroy();

        for (size_t j = i;
             j + 1 < policy_count;
             ++j) {

            policies[j] =
                policies[j + 1];
        }

        policies[policy_count - 1] =
            nullptr;

        --policy_count;

        return true;
    }

    return false;
}

// ============================================================
// Policy selection
// ============================================================

bool select_policy(
    const char* name
) {
    if (!name)
        return false;

    for (size_t i = 0; i < policy_count; ++i) {

        const Policy* policy =
            policies[i];

        if (!policy)
            continue;

        if (strcmp(
                policy->name,
                name
            ) != 0) {

            continue;
        }

        if (active_policy == policy)
            return true;

        active_policy = policy;

        print_policy(
            policy->name
        );

        return true;
    }

    return false;
}

const Policy*
current_policy() {
    return active_policy;
}

const char*
current_policy_name() {
    if (!active_policy)
        return nullptr;

    return active_policy->name;
}

// ============================================================
// Pick next
// ============================================================

Task* pick_next()
{
    if (!active_policy ||
        !active_policy->pick_next) {

        return nullptr;
    }

    return active_policy->pick_next(
        current
    );
}

// ============================================================
// Scheduler decision
// ============================================================

Decision make_decision()
{
    Decision result{};

    Task* candidate =
        pick_next();

    if (!candidate) {
        result.task_id = 0;
        result.run = false;
        result.preempt_current = false;

        return result;
    }

    result.task_id =
        candidate->id;

    result.run = true;

    result.preempt_current = false;

    if (active_policy &&
        active_policy->should_preempt) {

        result.preempt_current =
            active_policy->should_preempt(
                current,
                candidate
            );
    }

    return result;
}

// ============================================================
// Tick
// ============================================================

void scheduler_tick(
    uint64_t elapsed_ns
) {
    scheduler_time_ns +=
        elapsed_ns;

    if (current &&
        current->state ==
            TaskState::Running) {

        current->runtime_ns +=
            elapsed_ns;
    }

    Decision decision =
        make_decision();

    if (!decision.run)
        return;

    Task* next =
        find_task(
            decision.task_id
        );

    if (!next)
        return;

    /*
     * A real preemptive implementation will perform the
     * architecture-specific register/context switch here.
     *
     * This scheduler layer only owns policy.
     */

    if (!current ||
        decision.preempt_current) {

        if (current &&
            current != next &&
            current->state ==
                TaskState::Running) {

            current->state =
                TaskState::Ready;
        }

        current = next;

        current->state =
            TaskState::Running;

        if (active_policy &&
            active_policy->task_running) {

            active_policy->task_running(
                current
            );
        }
    }
}

// ============================================================
// State helpers
// ============================================================

bool set_ready(
    uint64_t task_id
) {
    Task* task =
        find_task(task_id);

    if (!task)
        return false;

    task->state =
        TaskState::Ready;

    return true;
}

bool set_running(
    uint64_t task_id
) {
    Task* task =
        find_task(task_id);

    if (!task)
        return false;

    current = task;

    task->state =
        TaskState::Running;

    if (active_policy &&
        active_policy->task_running) {

        active_policy->task_running(
            task
        );
    }

    return true;
}

bool set_sleeping(
    uint64_t task_id
) {
    Task* task =
        find_task(task_id);

    if (!task)
        return false;

    task->state =
        TaskState::Sleeping;

    return true;
}

bool set_blocked(
    uint64_t task_id
) {
    Task* task =
        find_task(task_id);

    if (!task)
        return false;

    task->state =
        TaskState::Blocked;

    if (active_policy &&
        active_policy->task_blocked) {

        active_policy->task_blocked(
            task
        );
    }

    if (current == task)
        current = nullptr;

    return true;
}

bool set_finished(
    uint64_t task_id
) {
    Task* task =
        find_task(task_id);

    if (!task)
        return false;

    task->state =
        TaskState::Finished;

    if (active_policy &&
        active_policy->task_finished) {

        active_policy->task_finished(
            task
        );
    }

    if (current == task)
        current = nullptr;

    return true;
}

// ============================================================
// Legacy cooperative API
// ============================================================

void yield()
{
    if (!current)
        return;

    current->state =
        TaskState::Ready;

    Task* next =
        pick_next();

    if (!next)
        return;

    current = next;

    current->state =
        TaskState::Running;
}

void run_scheduler_loop()
{
    for (;;) {

        Decision decision =
            make_decision();

        if (!decision.run)
            break;

        Task* task =
            find_task(
                decision.task_id
            );

        if (!task)
            break;

        current = task;

        task->state =
            TaskState::Running;

        if (active_policy &&
            active_policy->task_running) {

            active_policy->task_running(
                task
            );
        }

        if (task->entry)
            task->entry(task->arg);

        task->state =
            TaskState::Finished;

        if (active_policy &&
            active_policy->task_finished) {

            active_policy->task_finished(
                task
            );
        }

        if (current == task)
            current = nullptr;
    }
}

} // namespace scheduler
