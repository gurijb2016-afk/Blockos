#include "scheduler.hpp"

namespace blockos_custom_scheduler {

static bool init()
{
    return true;
}

static void destroy()
{
}

static bool enqueue(
    scheduler::Task*
)
{
    return true;
}

static void dequeue(
    scheduler::Task*
)
{
}

static scheduler::Task*
pick_next(
    scheduler::Task* current
) {
    scheduler::Task* best = nullptr;

    for (size_t i = 0;
         i < scheduler::task_count();
         ++i) {

        (void)i;
    }

    /*
     * A tényleges runqueue traversalt a kernel
     * scheduler core-ján keresztül kell megadni.
     */

    return current ? current : best;
}

static void running(
    scheduler::Task*
)
{
}

static void blocked(
    scheduler::Task*
)
{
}

static void finished(
    scheduler::Task*
)
{
}

static bool should_preempt(
    scheduler::Task* current,
    scheduler::Task* candidate
) {
    if (!candidate)
        return false;

    if (!current)
        return true;

    return candidate->deadline_ns != 0 &&
           current->deadline_ns != 0 &&
           candidate->deadline_ns <
           current->deadline_ns;
}

static const scheduler::Policy policy = {
    "blockos-deadline",

    init,
    destroy,

    enqueue,
    dequeue,

    pick_next,

    running,
    blocked,
    finished,

    should_preempt
};

bool install()
{
    return scheduler::register_policy(
        &policy
    );
}

} // namespace blockos_custom_scheduler
