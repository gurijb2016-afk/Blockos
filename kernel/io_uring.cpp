#include "kernel/io_uring.hpp"
#include "spinlock.hpp"

#include <cstdint>
#include <cstddef>

namespace Blockos {

// ============================================================
// Kernel memóriafoglalás
// ============================================================

void* io_uring::allocate(size_t size)
{
    return ::operator new(size);
}

void io_uring::deallocate(void* ptr)
{
    ::operator delete(ptr);
}

// ============================================================
// Konstruktor
// ============================================================

io_uring::io_uring(uint32_t depth)
    : m_lock(nullptr),
      m_sqes(nullptr),
      m_cqes(nullptr),
      m_queue_depth(depth),
      m_sq_head(nullptr),
      m_sq_tail(nullptr),
      m_cq_head(nullptr),
      m_cq_tail(nullptr),
      m_running(false)
{
    if (depth == 0)
        return;

    m_lock = new Spinlock();

    m_sqes = reinterpret_cast<io_uring_sqe*>(
        allocate(sizeof(io_uring_sqe) * depth)
    );

    m_cqes = reinterpret_cast<io_uring_cqe*>(
        allocate(sizeof(io_uring_cqe) * depth)
    );

    m_sq_head = reinterpret_cast<volatile uint32_t*>(
        allocate(sizeof(uint32_t))
    );

    m_sq_tail = reinterpret_cast<volatile uint32_t*>(
        allocate(sizeof(uint32_t))
    );

    m_cq_head = reinterpret_cast<volatile uint32_t*>(
        allocate(sizeof(uint32_t))
    );

    m_cq_tail = reinterpret_cast<volatile uint32_t*>(
        allocate(sizeof(uint32_t))
    );

    if (!m_sqes ||
        !m_cqes ||
        !m_sq_head ||
        !m_sq_tail ||
        !m_cq_head ||
        !m_cq_tail) {

        m_running = false;
        return;
    }

    *m_sq_head = 0;
    *m_sq_tail = 0;
    *m_cq_head = 0;
    *m_cq_tail = 0;

    for (uint32_t i = 0; i < depth; ++i) {
        m_sqes[i] = {};
        m_cqes[i] = {};
    }
}

// ============================================================
// Destruktor
// ============================================================

io_uring::~io_uring()
{
    m_running = false;

    if (m_sqes)
        deallocate(m_sqes);

    if (m_cqes)
        deallocate(m_cqes);

    if (m_sq_head)
        deallocate(const_cast<uint32_t*>(m_sq_head));

    if (m_sq_tail)
        deallocate(const_cast<uint32_t*>(m_sq_tail));

    if (m_cq_head)
        deallocate(const_cast<uint32_t*>(m_cq_head));

    if (m_cq_tail)
        deallocate(const_cast<uint32_t*>(m_cq_tail));

    delete m_lock;

    m_sqes = nullptr;
    m_cqes = nullptr;

    m_sq_head = nullptr;
    m_sq_tail = nullptr;
    m_cq_head = nullptr;
    m_cq_tail = nullptr;

    m_lock = nullptr;
}

// ============================================================
// Queue index
// ============================================================

uint32_t io_uring::next_index(uint32_t index) const
{
    if (m_queue_depth == 0)
        return 0;

    return index % m_queue_depth;
}

// ============================================================
// SQE végrehajtása
// ============================================================

int32_t io_uring::process_sqe(const io_uring_sqe& sqe)
{
    switch (sqe.opcode) {

        case 0:
            // READ
            //
            // Később ide kerülhet:
            // VFS read -> filesystem -> block device
            //
            return 0;

        case 1:
            // WRITE
            //
            // Később:
            // VFS write -> filesystem -> block device
            //
            return 0;

        case 2:
            // NETWORK RECEIVE
            return 0;

        default:
            return -1;
    }
}

// ============================================================
// Start
// ============================================================

void io_uring::start()
{
    if (m_queue_depth == 0)
        return;

    if (!m_sqes || !m_cqes)
        return;

    m_running = true;
}

// ============================================================
// Stop
// ============================================================

void io_uring::stop()
{
    m_running = false;
}

// ============================================================
// Running
// ============================================================

bool io_uring::running() const
{
    return m_running;
}

// ============================================================
// Submit
// ============================================================

bool io_uring::submit(
    uint8_t opcode,
    int32_t fd,
    uint64_t addr,
    uint32_t len,
    uint64_t user_data)
{
    if (!m_running)
        return false;

    if (!m_sqes ||
        !m_sq_head ||
        !m_sq_tail)
        return false;

    ScopedLock guard(*m_lock);

    uint32_t head = *m_sq_head;
    uint32_t tail = *m_sq_tail;

    if ((tail - head) >= m_queue_depth)
        return false;

    uint32_t index = next_index(tail);

    m_sqes[index].opcode = opcode;
    m_sqes[index].fd = fd;
    m_sqes[index].addr = addr;
    m_sqes[index].len = len;
    m_sqes[index].user_data = user_data;

    __atomic_store_n(
        m_sq_tail,
        tail + 1,
        __ATOMIC_RELEASE
    );

    return true;
}

// ============================================================
// Poll
// ============================================================

void io_uring::poll()
{
    if (!m_running)
        return;

    if (!m_sqes ||
        !m_cqes ||
        !m_sq_head ||
        !m_sq_tail ||
        !m_cq_head ||
        !m_cq_tail)
        return;

    while (true) {

        uint32_t sq_head =
            __atomic_load_n(
                m_sq_head,
                __ATOMIC_ACQUIRE
            );

        uint32_t sq_tail =
            __atomic_load_n(
                m_sq_tail,
                __ATOMIC_ACQUIRE
            );

        if (sq_head == sq_tail)
            break;

        uint32_t sq_index =
            next_index(sq_head);

        io_uring_sqe sqe =
            m_sqes[sq_index];

        __atomic_store_n(
            m_sq_head,
            sq_head + 1,
            __ATOMIC_RELEASE
        );

        int32_t result =
            process_sqe(sqe);

        uint32_t cq_head =
            __atomic_load_n(
                m_cq_head,
                __ATOMIC_ACQUIRE
            );

        uint32_t cq_tail =
            __atomic_load_n(
                m_cq_tail,
                __ATOMIC_ACQUIRE
            );

        if ((cq_tail - cq_head) >= m_queue_depth)
            break;

        uint32_t cq_index =
            next_index(cq_tail);

        m_cqes[cq_index].user_data =
            sqe.user_data;

        m_cqes[cq_index].res =
            result;

        __atomic_store_n(
            m_cq_tail,
            cq_tail + 1,
            __ATOMIC_RELEASE
        );
    }
}

// ============================================================
// Completion olvasás
// ============================================================

bool io_uring::get_completion(
    uint64_t& user_data,
    int32_t& result)
{
    if (!m_cqes ||
        !m_cq_head ||
        !m_cq_tail)
        return false;

    ScopedLock guard(*m_lock);

    uint32_t head =
        __atomic_load_n(
            m_cq_head,
            __ATOMIC_ACQUIRE
        );

    uint32_t tail =
        __atomic_load_n(
            m_cq_tail,
            __ATOMIC_ACQUIRE
        );

    if (head == tail)
        return false;

    uint32_t index =
        next_index(head);

    user_data =
        m_cqes[index].user_data;

    result =
        m_cqes[index].res;

    __atomic_store_n(
        m_cq_head,
        head + 1,
        __ATOMIC_RELEASE
    );

    return true;
}

// ============================================================
// Pending
// ============================================================

uint32_t io_uring::pending() const
{
    if (!m_sq_head || !m_sq_tail)
        return 0;

    return *m_sq_tail - *m_sq_head;
}

// ============================================================
// Completions
// ============================================================

uint32_t io_uring::completions() const
{
    if (!m_cq_head || !m_cq_tail)
        return 0;

    return *m_cq_tail - *m_cq_head;
}

// ============================================================
// Queue depth
// ============================================================

uint32_t io_uring::depth() const
{
    return m_queue_depth;
}

} // namespace Blockos
