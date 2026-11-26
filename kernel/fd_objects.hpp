#pragma once
/*
 * Kernel objects behind eventfd, timerfd and signalfd file descriptors.
 *
 * Each fd keeps an index into one of the fixed pools below (RuntimeFd::object).
 * Objects are not freed on close: a dup() or fork() may still reference them,
 * and the kernel does not track those references yet. The pools are sized for
 * the processes BlockOS runs today.
 *
 * Errors are returned as negative errno values, like the rest of the syscall layer.
 */
#include <stdint.h>
#include <stddef.h>

namespace fdobj {

constexpr int EVENTFD_POOL = 256;
constexpr int TIMERFD_POOL = 128;
constexpr int SIGNALFD_POOL = 64;

/* eventfd */
int eventfd_create(uint64_t initval, bool semaphore);            /* index or negative errno */
int64_t eventfd_read(int idx, uint64_t* out, bool nonblock);     /* 0 or negative errno */
int64_t eventfd_write(int idx, uint64_t value);                  /* 0 or negative errno */
bool eventfd_readable(int idx);
bool eventfd_writable(int idx);

/* timerfd. Times are kernel uptime in milliseconds (the clock behind clock_gettime). */
int timerfd_create_obj();                                        /* index or negative errno */
int64_t timerfd_settime(int idx, bool abstime, uint64_t value_ms,
                        uint64_t interval_ms, uint64_t* old_value_ms,
                        uint64_t* old_interval_ms);              /* 0 or negative errno */
int64_t timerfd_read(int idx, uint64_t* out, bool nonblock);     /* 0 or negative errno */
int64_t timerfd_gettime(int idx, uint64_t* value_ms, uint64_t* interval_ms);
bool timerfd_readable(int idx);

/* signalfd. Signals reach a signalfd through signal_raise(); the kernel does not
 * deliver signals asynchronously, so only the raised bits are visible. */
int signalfd_create(uint64_t owner_pid, uint64_t mask);          /* index or negative errno */
void signalfd_set_mask(int idx, uint64_t mask);
int64_t signalfd_read(int idx, void* out, size_t out_size, bool nonblock); /* bytes or negative errno */
bool signalfd_readable(int idx);
void signal_raise(uint64_t pid, int signo);                      /* mark signo pending on owner's signalfds */

} // namespace fdobj
