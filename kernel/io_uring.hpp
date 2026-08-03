#pragma once

#include <cstdint>
#include <cstddef>

#include "kernel/allocator.hpp"
#include "spinlock.hpp"

namespace Blockos {

struct io_uring_sqe {
    uint8_t  opcode;
    int32_t  fd;
    uint64_t addr;
    uint32_t len;
    uint64_t user_data;
};

struct io_uring_cqe {
    uint64_t user_data;
    int32_t  res;
};

class io_uring {
private:
    Spinlock* m_lock;

    io_uring_sqe* m_sqes;
    io_uring_cqe* m_cqes;

    uint32_t m_queue_depth;

    volatile uint32_t* m_sq_head;
    volatile uint32_t* m_sq_tail;
    volatile uint32_t* m_cq_head;
    volatile uint32_t* m_cq_tail;

    bool m_running;

    void* allocate(size_t size);
    void deallocate(void* ptr);

    uint32_t next_index(uint32_t index) const;

    int32_t process_sqe(const io_uring_sqe& sqe);

public:
    explicit io_uring(uint32_t depth);
    ~io_uring();

    void start();
    void stop();

    bool running() const;

    bool submit(
        uint8_t opcode,
        int32_t fd,
        uint64_t addr,
        uint32_t len,
        uint64_t user_data
    );

    void poll();

    bool get_completion(
        uint64_t& user_data,
        int32_t& result
    );

    uint32_t pending() const;
    uint32_t completions() const;
    uint32_t depth() const;
};

} // namespace Blockos
