#pragma once

#include <stdint.h>
#include <stddef.h>

#include "spinlock.hpp"

namespace Blockos {

struct rcu_head
{
    rcu_head* next;
    void (*callback)(void* obj);
    void* obj_ptr;
};


class RCU
{
private:

    Spinlock m_lock;

    volatile uint64_t m_global_generation;

    volatile uint64_t m_cpu_generations[64];

    size_t m_registered_cpus;

    rcu_head* m_pending_deletions;


public:

    RCU();


    void read_lock();

    void read_unlock();


    void call_rcu(
        rcu_head* head,
        void* obj,
        void (*cleanup_func)(void*)
    );


    void synchronize_rcu();
};

} // namespace Blockos