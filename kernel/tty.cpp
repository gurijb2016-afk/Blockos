

#include "../fs/vfs.hpp"

#include <cstddef>
#include <cstdint>
#include <string.h>

namespace blockos::tty {

static constexpr char DEVICE_PATH[] = "/devices/console0";
static constexpr std::size_t INPUT_CAPACITY = 4096;
static constexpr std::size_t LINE_CAPACITY  = 4096;

static constexpr std::int64_t TTY_OK     = 0;
static constexpr std::int64_t TTY_EINVAL = -22;
static constexpr std::int64_t TTY_ENOTTY = -25;
static constexpr std::int64_t TTY_EAGAIN = -11;

class SpinLock {
private:
    volatile std::uint32_t value_ = 0;

public:
    void lock() {
        while (__atomic_exchange_n(&value_, 1u, __ATOMIC_ACQUIRE) != 0u) {
            while (__atomic_load_n(&value_, __ATOMIC_RELAXED) != 0u) {
#if defined(__x86_64__) || defined(__i386__)
                __asm__ volatile("pause");
#elif defined(__aarch64__)
                __asm__ volatile("yield");
#endif
            }
        }
    }

    void unlock() {
        __atomic_store_n(&value_, 0u, __ATOMIC_RELEASE);
    }
};

class LockGuard {
    SpinLock& lock_;

public:
    explicit LockGuard(SpinLock& lock) : lock_(lock) {
        lock_.lock();
    }

    ~LockGuard() {
        lock_.unlock();
    }

    LockGuard(const LockGuard&) = delete;
    LockGuard& operator=(const LockGuard&) = delete;
};

enum LocalFlag : std::uint32_t {
    LFLAG_ECHO   = 1u << 0,
    LFLAG_ECHOE  = 1u << 1,
    LFLAG_ECHOK  = 1u << 2,
    LFLAG_ICANON = 1u << 3,
    LFLAG_ISIG   = 1u << 4,
    LFLAG_IEXTEN = 1u << 5
};

enum InputFlag : std::uint32_t {
    IFLAG_ICRNL  = 1u << 0,
    IFLAG_INLCR  = 1u << 1,
    IFLAG_IGNCR  = 1u << 2,
    IFLAG_ISTRIP = 1u << 3
};

enum OutputFlag : std::uint32_t {
    OFLAG_OPOST = 1u << 0,
    OFLAG_ONLCR = 1u << 1
};

struct WindowSize {
    std::uint16_t rows;
    std::uint16_t cols;
    std::uint16_t xpixel;
    std::uint16_t ypixel;
};

struct Termios {
    std::uint32_t iflag;
    std::uint32_t oflag;
    std::uint32_t cflag;
    std::uint32_t lflag;
    std::uint8_t cc[20];
};

static constexpr std::uint64_t IOCTL_GET_WINSZ =
    0x424F5301ULL;

static constexpr std::uint64_t IOCTL_SET_WINSZ =
    0x424F5302ULL;

static constexpr std::uint64_t IOCTL_GET_TERMIOS =
    0x424F5303ULL;

static constexpr std::uint64_t IOCTL_SET_TERMIOS =
    0x424F5304ULL;

static constexpr std::uint64_t IOCTL_GET_PGRP =
    0x424F5305ULL;

static constexpr std::uint64_t IOCTL_SET_PGRP =
    0x424F5306ULL;

static constexpr std::uint64_t IOCTL_FLUSH =
    0x424F5307ULL;

static constexpr std::uint64_t IOCTL_SET_NONBLOCK =
    0x424F5308ULL;

static constexpr std::uint64_t IOCTL_GET_NONBLOCK =
    0x424F5309ULL;

static constexpr std::uint8_t CC_VINTR  = 3;
static constexpr std::uint8_t CC_VEOF   = 4;
static constexpr std::uint8_t CC_VERASE = 127;
static constexpr std::uint8_t CC_VKILL  = 21;
static constexpr std::uint8_t CC_VSTART = 17;
static constexpr std::uint8_t CC_VSTOP  = 19;
static constexpr std::uint8_t CC_VSUSP  = 26;

using OutputCallback =
    void (*)(const char*, std::size_t, void*);

using SignalCallback =
    void (*)(int, std::uint64_t, void*);

static void default_output(
    const char* data,
    std::size_t length,
    void*)
{
    (void)data;
    (void)length;
}

static void default_signal(
    int,
    std::uint64_t,
    void*)
{
}

class RingBuffer {
private:
    std::uint8_t buffer_[INPUT_CAPACITY]{};
    std::size_t head_ = 0;
    std::size_t tail_ = 0;
    std::size_t count_ = 0;

public:
    bool push(std::uint8_t value) {
        if (count_ >= INPUT_CAPACITY)
            return false;

        buffer_[head_] = value;
        head_ = (head_ + 1) % INPUT_CAPACITY;
        ++count_;

        return true;
    }

    bool pop(std::uint8_t& value) {
        if (count_ == 0)
            return false;

        value = buffer_[tail_];
        tail_ = (tail_ + 1) % INPUT_CAPACITY;
        --count_;

        return true;
    }

    std::size_t size() const {
        return count_;
    }

    void clear() {
        head_ = 0;
        tail_ = 0;
        count_ = 0;
    }
};

class ConsoleTTY {
private:
    SpinLock lock_;
    RingBuffer input_;

    OutputCallback output_ = default_output;
    SignalCallback signal_ = default_signal;
    void* callback_user_ = nullptr;

    Termios termios_{};
    WindowSize winsz_{25, 80, 0, 0};

    std::uint64_t session_id_ = 1;
    std::uint64_t foreground_pgrp_ = 1;

    bool nonblock_ = false;
    bool initialized_ = false;

    char line_[LINE_CAPACITY]{};
    std::size_t line_len_ = 0;
    bool line_ready_ = false;

    void output_raw_locked(
        const char* data,
        std::size_t len)
    {
        if (!output_ || !data || len == 0)
            return;

        output_(data, len, callback_user_);
    }

    void output_char_locked(char c) {
        if ((termios_.oflag & OFLAG_OPOST) &&
            (termios_.oflag & OFLAG_ONLCR) &&
            c == '\n')
        {
            const char nl[] = "\r\n";
            output_raw_locked(nl, 2);
            return;
        }

        output_raw_locked(&c, 1);
    }

    void echo_char_locked(std::uint8_t c) {
        if (!(termios_.lflag & LFLAG_ECHO))
            return;

        if (c == '\n' ||
            c == '\r' ||
            c == '\t')
        {
            output_char_locked(static_cast<char>(c));
            return;
        }

        if (c == CC_VERASE) {
            if (termios_.lflag & LFLAG_ECHOE) {
                const char erase[] = "\b \b";
                output_raw_locked(erase, 3);
            }
            return;
        }

        if (c < 0x20u) {
            char caret[2];

            caret[0] = '^';
            caret[1] = static_cast<char>(c + '@');

            output_raw_locked(caret, 2);
            return;
        }

        output_char_locked(static_cast<char>(c));
    }

    void signal_locked(int signal) {
        const std::uint64_t pgrp = foreground_pgrp_;
        SignalCallback cb = signal_;
        void* user = callback_user_;

        lock_.unlock();

        if (cb)
            cb(signal, pgrp, user);

        lock_.lock();
    }

    bool push_canonical_locked(std::uint8_t c) {
        if (c == '\r') {
            if (termios_.iflag & IFLAG_ICRNL) {
                c = '\n';
            }
            else if (termios_.iflag & IFLAG_IGNCR) {
                return true;
            }
        }
        else if (c == '\n' &&
                 (termios_.iflag & IFLAG_INLCR))
        {
            c = '\r';
        }

        if (c == termios_.cc[CC_VINTR]) {
            if (termios_.lflag & LFLAG_ISIG)
                signal_locked(2);

            return true;
        }

        if (c == termios_.cc[CC_VSUSP]) {
            if (termios_.lflag & LFLAG_ISIG)
                signal_locked(20);

            return true;
        }

        if (c == termios_.cc[CC_VEOF]) {
            line_ready_ = true;
            return true;
        }

        if (c == termios_.cc[CC_VERASE]) {
            if (line_len_ != 0) {
                --line_len_;
                line_[line_len_] = '\0';
                echo_char_locked(c);
            }

            return true;
        }

        if (c == termios_.cc[CC_VKILL]) {
            line_len_ = 0;
            line_[0] = '\0';

            if (termios_.lflag & LFLAG_ECHOK) {
                const char nl[] = "\r\n";
                output_raw_locked(nl, 2);
            }

            return true;
        }

        if (c == termios_.cc[CC_VSTART] ||
            c == termios_.cc[CC_VSTOP])
        {
            return true;
        }

        if (termios_.iflag & IFLAG_ISTRIP)
            c &= 0x7Fu;

        if (line_len_ + 1 >= LINE_CAPACITY)
            return false;

        line_[line_len_++] = static_cast<char>(c);
        line_[line_len_] = '\0';

        echo_char_locked(c);

        if (c == '\n')
            line_ready_ = true;

        return true;
    }

    bool publish_line_locked() {
        if (!line_ready_)
            return false;

        for (std::size_t i = 0; i < line_len_; ++i) {
            if (!input_.push(
                    static_cast<std::uint8_t>(line_[i])))
            {
                return false;
            }
        }

        line_len_ = 0;
        line_[0] = '\0';
        line_ready_ = false;

        return true;
    }

    std::size_t drain_input_locked(
        void* dst,
        std::size_t len)
    {
        if (!dst || len == 0)
            return 0;

        auto* bytes =
            static_cast<std::uint8_t*>(dst);

        std::size_t n = 0;

        while (n < len) {
            std::uint8_t c = 0;

            if (!input_.pop(c))
                break;

            bytes[n++] = c;
        }

        return n;
    }

public:
    void init() {
        LockGuard guard(lock_);

        if (initialized_)
            return;

        memset(&termios_, 0, sizeof(termios_));
        termios_.iflag = IFLAG_ICRNL;
        termios_.oflag = OFLAG_OPOST | OFLAG_ONLCR;
        termios_.cflag = 0;

        termios_.lflag =
            LFLAG_ECHO |
            LFLAG_ECHOE |
            LFLAG_ECHOK |
            LFLAG_ICANON |
            LFLAG_ISIG |
            LFLAG_IEXTEN;

        termios_.cc[CC_VINTR]  = 3;
        termios_.cc[CC_VEOF]   = 4;
        termios_.cc[CC_VERASE] = 127;
        termios_.cc[CC_VKILL]  = 21;
        termios_.cc[CC_VSTART] = 17;
        termios_.cc[CC_VSTOP]  = 19;
        termios_.cc[CC_VSUSP]  = 26;

        input_.clear();

        line_len_ = 0;
        line_ready_ = false;

        foreground_pgrp_ = session_id_;
        nonblock_ = false;
        initialized_ = true;
    }

    void set_output_callback(
        OutputCallback output,
        void* user)
    {
        LockGuard guard(lock_);

        output_ =
            output ? output : default_output;

        callback_user_ = user;
    }

    void set_signal_callback(
        SignalCallback signal,
        void* user)
    {
        LockGuard guard(lock_);

        signal_ =
            signal ? signal : default_signal;

        callback_user_ = user;
    }

    const char* device_path() const {
        return DEVICE_PATH;
    }

    std::int64_t write(
        const void* data,
        std::size_t len)
    {
        if (!data && len != 0)
            return TTY_EINVAL;

        if (!initialized_)
            init();

        LockGuard guard(lock_);

        const auto* bytes =
            static_cast<const char*>(data);

        for (std::size_t i = 0; i < len; ++i)
            output_char_locked(bytes[i]);

        return static_cast<std::int64_t>(len);
    }

    std::int64_t read(
        void* dst,
        std::size_t len)
    {
        if (!dst && len != 0)
            return TTY_EINVAL;

        if (len == 0)
            return 0;

        if (!initialized_)
            init();

        LockGuard guard(lock_);

        if (termios_.lflag & LFLAG_ICANON)
            publish_line_locked();

        if (input_.size() == 0)
            return nonblock_ ? TTY_EAGAIN : 0;

        return static_cast<std::int64_t>(
            drain_input_locked(dst, len));
    }

    void input_char(std::uint8_t c) {
        if (!initialized_)
            init();

        LockGuard guard(lock_);

        if (termios_.lflag & LFLAG_ICANON) {
            if (!push_canonical_locked(c))
                return;

            if (line_ready_)
                publish_line_locked();

            return;
        }

        if (c == '\r') {
            if (termios_.iflag & IFLAG_ICRNL)
                c = '\n';
            else if (termios_.iflag & IFLAG_IGNCR)
                return;
        }
        else if (c == '\n' &&
                 (termios_.iflag & IFLAG_INLCR))
        {
            c = '\r';
        }

        if (termios_.iflag & IFLAG_ISTRIP)
            c &= 0x7Fu;

        if (c == termios_.cc[CC_VINTR] &&
            (termios_.lflag & LFLAG_ISIG))
        {
            signal_locked(2);
            return;
        }

        if (!input_.push(c))
            return;

        echo_char_locked(c);
    }

    void input_bytes(
        const void* data,
        std::size_t len)
    {
        if (!data || len == 0)
            return;

        const auto* p =
            static_cast<const std::uint8_t*>(data);

        for (std::size_t i = 0; i < len; ++i)
            input_char(p[i]);
    }

    std::int64_t ioctl(
        std::uint64_t request,
        void* arg)
    {
        if (!initialized_)
            init();

        LockGuard guard(lock_);

        switch (request) {
        case IOCTL_GET_WINSZ:
            if (!arg)
                return TTY_EINVAL;

            *static_cast<WindowSize*>(arg) =
                winsz_;

            return TTY_OK;

        case IOCTL_SET_WINSZ:
            if (!arg)
                return TTY_EINVAL;

            winsz_ =
                *static_cast<const WindowSize*>(arg);

            if (winsz_.rows == 0)
                winsz_.rows = 25;

            if (winsz_.cols == 0)
                winsz_.cols = 80;

            return TTY_OK;

        case IOCTL_GET_TERMIOS:
            if (!arg)
                return TTY_EINVAL;

            *static_cast<Termios*>(arg) =
                termios_;

            return TTY_OK;

        case IOCTL_SET_TERMIOS:
            if (!arg)
                return TTY_EINVAL;

            termios_ =
                *static_cast<const Termios*>(arg);

            return TTY_OK;

        case IOCTL_GET_PGRP:
            if (!arg)
                return TTY_EINVAL;

            *static_cast<std::uint64_t*>(arg) =
                foreground_pgrp_;

            return TTY_OK;

        case IOCTL_SET_PGRP:
            if (!arg)
                return TTY_EINVAL;

            foreground_pgrp_ =
                *static_cast<const std::uint64_t*>(arg);

            return TTY_OK;

        case IOCTL_FLUSH:
            input_.clear();
            line_len_ = 0;
            line_[0] = '\0';
            line_ready_ = false;

            return TTY_OK;

        case IOCTL_SET_NONBLOCK:
            if (!arg)
                return TTY_EINVAL;

            nonblock_ =
                (*static_cast<const std::uint32_t*>(arg) != 0);

            return TTY_OK;

        case IOCTL_GET_NONBLOCK:
            if (!arg)
                return TTY_EINVAL;

            *static_cast<std::uint32_t*>(arg) =
                nonblock_ ? 1u : 0u;

            return TTY_OK;

        default:
            return TTY_ENOTTY;
        }
    }

    std::size_t available() const {
        return input_.size();
    }

    std::uint64_t foreground_pgrp() const {
        return foreground_pgrp_;
    }

    std::uint64_t session_id() const {
        return session_id_;
    }

    bool initialized() const {
        return initialized_;
    }
};

static ConsoleTTY g_console;

bool register_device() {
    if (!g_console.initialized())
        g_console.init();

    static constexpr std::uint8_t descriptor[] = {
        'B','l','o','c','k','O','S',' ',
        'c','o','n','s','o','l','e','0','\n'
    };

    std::uint32_t existing_size = 0;

    const std::uint8_t* existing =
        vfs::read_file(
            DEVICE_PATH,
            &existing_size);

    if (existing != nullptr)
        return true;

    return vfs::create_file(
        DEVICE_PATH,
        descriptor,
        static_cast<std::uint32_t>(
            sizeof(descriptor)));
}

ConsoleTTY& console() {
    if (!g_console.initialized())
        g_console.init();

    return g_console;
}

} // namespace blockos::tty

extern "C" {

void blockos_tty_init() {
    blockos::tty::console().init();
    (void)blockos::tty::register_device();
}

void blockos_tty_input_char(
    std::uint8_t c)
{
    blockos::tty::console().input_char(c);
}

void blockos_tty_input_bytes(
    const void* data,
    std::size_t len)
{
    blockos::tty::console().input_bytes(
        data,
        len);
}

std::int64_t blockos_tty_read(
    void* buffer,
    std::size_t length)
{
    return blockos::tty::console().read(
        buffer,
        length);
}

std::int64_t blockos_tty_write(
    const void* buffer,
    std::size_t length)
{
    return blockos::tty::console().write(
        buffer,
        length);
}

std::int64_t blockos_tty_ioctl(
    std::uint64_t request,
    void* arg)
{
    return blockos::tty::console().ioctl(
        request,
        arg);
}

void blockos_tty_set_output_callback(
    blockos::tty::OutputCallback callback,
    void* user)
{
    blockos::tty::console().set_output_callback(
        callback,
        user);
}

void blockos_tty_set_signal_callback(
    blockos::tty::SignalCallback callback,
    void* user)
{
    blockos::tty::console().set_signal_callback(
        callback,
        user);
}

void blockos_terminal_print(
    const char* text,
    std::size_t length)
{
    (void)blockos_tty_write(text, length);
}

}
