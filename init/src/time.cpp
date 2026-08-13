#include "init/time.hpp"

#include <time.h>

namespace blockos::init {

uint64_t monotonic_ms()
{
    struct timespec ts{};

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0;

    return static_cast<uint64_t>(ts.tv_sec) * 1000ULL +
           static_cast<uint64_t>(ts.tv_nsec) / 1000000ULL;
}

void sleep_ms(uint32_t milliseconds)
{
    struct timespec request{};

    request.tv_sec = milliseconds / 1000U;
    request.tv_nsec =
        static_cast<long>(milliseconds % 1000U) * 1000000L;

    nanosleep(&request, nullptr);
}

} // namespace blockos::init
