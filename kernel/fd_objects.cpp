#include "fd_objects.hpp"
#include <string.h>

extern "C" uint64_t timer_uptime_ms();

namespace fdobj {
namespace {

constexpr int64_t E_AGAIN = -11;
constexpr int64_t E_INVAL = -22;
constexpr int64_t E_NFILE = -23;
constexpr uint64_t COUNTER_MAX = 0xFFFFFFFFFFFFFFFEULL;

struct Eventfd {
    bool used;
    bool semaphore;
    uint64_t counter;
};

struct Timerfd {
    bool used;
    bool armed;
    uint64_t next_ms;       /* absolute uptime of the next expiration */
    uint64_t interval_ms;   /* 0 = one-shot */
    uint64_t expirations;   /* not yet read */
};

struct Signalfd {
    bool used;
    uint64_t owner_pid;
    uint64_t mask;          /* bit (signo-1) */
    uint64_t pending;       /* raised, not yet read; bit (signo-1) */
};

Eventfd eventfds[EVENTFD_POOL];
Timerfd timerfds[TIMERFD_POOL];
Signalfd signalfds[SIGNALFD_POOL];

uint64_t sig_bit(int signo) {
    return (signo >= 1 && signo <= 64) ? (1ULL << (signo - 1)) : 0;
}

/* Brings a timer's expiration count up to date with the current uptime. */
void timer_refresh(Timerfd& t, uint64_t now) {
    if (!t.armed || now < t.next_ms)
        return;
    if (t.interval_ms == 0) {
        t.expirations += 1;
        t.armed = false;
        return;
    }
    uint64_t periods = (now - t.next_ms) / t.interval_ms + 1;
    t.expirations += periods;
    t.next_ms += periods * t.interval_ms;
}

} // namespace

/* ---------------- eventfd ---------------- */

int eventfd_create(uint64_t initval, bool semaphore) {
    for (int i = 0; i < EVENTFD_POOL; ++i) {
        if (!eventfds[i].used) {
            eventfds[i] = {true, semaphore, initval};
            return i;
        }
    }
    return (int)E_NFILE;
}

int64_t eventfd_read(int idx, uint64_t* out, bool nonblock) {
    if (idx < 0 || idx >= EVENTFD_POOL || !eventfds[idx].used || !out)
        return E_INVAL;
    Eventfd& e = eventfds[idx];
    if (e.counter == 0)
        return E_AGAIN; /* a blocking read would sleep here; reported as EAGAIN for now */
    if (e.semaphore) {
        *out = 1;
        e.counter -= 1;
    } else {
        *out = e.counter;
        e.counter = 0;
    }
    (void)nonblock;
    return 0;
}

int64_t eventfd_write(int idx, uint64_t value) {
    if (idx < 0 || idx >= EVENTFD_POOL || !eventfds[idx].used)
        return E_INVAL;
    if (value == 0xFFFFFFFFFFFFFFFFULL)
        return E_INVAL;
    Eventfd& e = eventfds[idx];
    if (value > COUNTER_MAX - e.counter)
        return E_AGAIN;
    e.counter += value;
    return 0;
}

bool eventfd_readable(int idx) {
    return idx >= 0 && idx < EVENTFD_POOL && eventfds[idx].used && eventfds[idx].counter > 0;
}

bool eventfd_writable(int idx) {
    return idx >= 0 && idx < EVENTFD_POOL && eventfds[idx].used && eventfds[idx].counter < COUNTER_MAX;
}

/* ---------------- timerfd ---------------- */

int timerfd_create_obj() {
    for (int i = 0; i < TIMERFD_POOL; ++i) {
        if (!timerfds[i].used) {
            timerfds[i] = {true, false, 0, 0, 0};
            return i;
        }
    }
    return (int)E_NFILE;
}

int64_t timerfd_settime(int idx, bool abstime, uint64_t value_ms,
                        uint64_t interval_ms, uint64_t* old_value_ms,
                        uint64_t* old_interval_ms) {
    if (idx < 0 || idx >= TIMERFD_POOL || !timerfds[idx].used)
        return E_INVAL;
    Timerfd& t = timerfds[idx];
    uint64_t now = timer_uptime_ms();
    timer_refresh(t, now);

    if (old_value_ms)
        *old_value_ms = (t.armed && t.next_ms > now) ? t.next_ms - now : 0;
    if (old_interval_ms)
        *old_interval_ms = t.interval_ms;

    t.expirations = 0;
    t.interval_ms = interval_ms;
    if (value_ms == 0) {
        t.armed = false; /* a zero it_value disarms the timer */
        return 0;
    }
    t.next_ms = abstime ? value_ms : now + value_ms;
    t.armed = true;
    return 0;
}

int64_t timerfd_read(int idx, uint64_t* out, bool nonblock) {
    if (idx < 0 || idx >= TIMERFD_POOL || !timerfds[idx].used || !out)
        return E_INVAL;
    Timerfd& t = timerfds[idx];
    timer_refresh(t, timer_uptime_ms());
    if (t.expirations == 0)
        return E_AGAIN;
    *out = t.expirations;
    t.expirations = 0;
    (void)nonblock;
    return 0;
}

/* Remaining time until the next expiry and the interval, both in ms (0 if disarmed). */
int64_t timerfd_gettime(int idx, uint64_t* value_ms, uint64_t* interval_ms) {
    if (idx < 0 || idx >= TIMERFD_POOL || !timerfds[idx].used || !value_ms || !interval_ms)
        return E_INVAL;
    Timerfd& t = timerfds[idx];
    uint64_t now = timer_uptime_ms();
    timer_refresh(t, now);
    *value_ms = (t.armed && t.next_ms > now) ? t.next_ms - now : 0;
    *interval_ms = t.interval_ms;
    return 0;
}

bool timerfd_readable(int idx) {
    if (idx < 0 || idx >= TIMERFD_POOL || !timerfds[idx].used)
        return false;
    timer_refresh(timerfds[idx], timer_uptime_ms());
    return timerfds[idx].expirations > 0;
}

/* ---------------- signalfd ---------------- */

int signalfd_create(uint64_t owner_pid, uint64_t mask) {
    for (int i = 0; i < SIGNALFD_POOL; ++i) {
        if (!signalfds[i].used) {
            signalfds[i] = {true, owner_pid, mask, 0};
            return i;
        }
    }
    return (int)E_NFILE;
}

void signalfd_set_mask(int idx, uint64_t mask) {
    if (idx >= 0 && idx < SIGNALFD_POOL && signalfds[idx].used)
        signalfds[idx].mask = mask;
}

bool signalfd_readable(int idx) {
    return idx >= 0 && idx < SIGNALFD_POOL && signalfds[idx].used &&
           (signalfds[idx].pending & signalfds[idx].mask) != 0;
}

/* struct signalfd_siginfo is 128 bytes; ssi_signo is the first field. */
int64_t signalfd_read(int idx, void* out, size_t out_size, bool nonblock) {
    if (idx < 0 || idx >= SIGNALFD_POOL || !signalfds[idx].used || !out)
        return E_INVAL;
    if (out_size < 128)
        return E_INVAL;
    Signalfd& s = signalfds[idx];
    uint64_t ready = s.pending & s.mask;
    if (!ready)
        return E_AGAIN;
    int signo = 1;
    while (!(ready & 1ULL)) {
        ready >>= 1;
        ++signo;
    }
    s.pending &= ~sig_bit(signo);
    memset(out, 0, 128);
    uint32_t signo32 = (uint32_t)signo;
    memcpy(out, &signo32, sizeof(signo32));
    (void)nonblock;
    return 128;
}

void signal_raise(uint64_t pid, int signo) {
    uint64_t bit = sig_bit(signo);
    if (!bit)
        return;
    for (int i = 0; i < SIGNALFD_POOL; ++i) {
        if (signalfds[i].used && signalfds[i].owner_pid == pid && (signalfds[i].mask & bit))
            signalfds[i].pending |= bit;
    }
}

} // namespace fdobj
