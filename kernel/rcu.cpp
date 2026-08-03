#include "rcu.hpp"
#include "spinlock.hpp"

#include <stdint.h>
#include <stddef.h>

namespace Blockos {

RCU::RCU()
    : m_global_generation(0),
      m_registered_cpus(1),
      m_pending_deletions(nullptr)
{
    for (size_t i = 0; i < 64; ++i)
        m_cpu_generations[i] = 0;
}


void RCU::read_lock()
{
    /*
     * A jelenlegi BlockOS scheduler egyelőre single-core
     * kooperatív scheduler, ezért CPU ID = 0.
     */
    const size_t cpu_id = 0;

    uint64_t gen =
        __atomic_load_n(
            &m_global_generation,
            __ATOMIC_ACQUIRE
        );

    __atomic_store_n(
        &m_cpu_generations[cpu_id],
        gen,
        __ATOMIC_RELEASE
    );
}


void RCU::read_unlock()
{
    const size_t cpu_id = 0;

    __atomic_store_n(
        &m_cpu_generations[cpu_id],
        0,
        __ATOMIC_RELEASE
    );
}


void RCU::call_rcu(
    rcu_head* head,
    void* obj,
    void (*cleanup_func)(void*)
)
{
    if (!head || !cleanup_func)
        return;

    head->obj_ptr = obj;
    head->callback = cleanup_func;

    ScopedLock guard(m_lock);

    head->next = m_pending_deletions;
    m_pending_deletions = head;
}


void RCU::synchronize_rcu()
{
    /*
     * Move to the next RCU generation.
     */
    const uint64_t old_gen =
        __atomic_fetch_add(
            &m_global_generation,
            1,
            __ATOMIC_SEQ_CST
        );


    /*
     * Wait until every registered CPU has left
     * the old RCU read-side critical section.
     *
     * Current BlockOS scheduler is single-core,
     * but this structure is already prepared for
     * up to 64 CPUs.
     */
    for (size_t i = 0; i < m_registered_cpus; ++i)
    {
        while (true)
        {
            const uint64_t cpu_gen =
                __atomic_load_n(
                    &m_cpu_generations[i],
                    __ATOMIC_ACQUIRE
                );

            if (cpu_gen == 0 || cpu_gen > old_gen)
                break;

            asm volatile("pause");
        }
    }


    /*
     * Detach pending deletion list.
     */
    rcu_head* to_delete = nullptr;

    {
        ScopedLock guard(m_lock);

        to_delete = m_pending_deletions;
        m_pending_deletions = nullptr;
    }


    /*
     * Execute callbacks outside the lock.
     */
    while (to_delete)
    {
        rcu_head* next = to_delete->next;

        if (to_delete->callback)
            to_delete->callback(to_delete->obj_ptr);

        to_delete = next;
    }
}

} // namespace Blockos