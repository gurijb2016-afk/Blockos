#include <stdio.h>

#include "command.hpp"
#include "irq.hpp"

namespace blockos::cmd
{

extern "C" int uptime_main(const Args& args, Console& out)
{
    (void) args;
    (void) out;

    if (timer_divisor() == 0)
    {
        printf("uptime: timer not initialized\n");
        return 1;
    }

    const uint64_t ms = timer_uptime_ms();

    const uint64_t milliseconds = ms % 1000u;
    const uint64_t total_seconds = ms / 1000u;
    const uint64_t seconds = total_seconds % 60u;
    const uint64_t total_minutes = total_seconds / 60u;
    const uint64_t minutes = total_minutes % 60u;
    const uint64_t total_hours = total_minutes / 60u;
    const uint64_t hours = total_hours % 24u;
    const uint64_t days = total_hours / 24u;

    if (days > 0)
    {
        printf(
            "up %llud %02llu:%02llu:%02llu.%03llu\n",
            days,
            hours,
            minutes,
            seconds,
            milliseconds);
    }
    else
    {
        printf(
            "time since boot: %02llu:%02llu:%02llu.%03llu\n",
            hours,
            minutes,
            seconds,
            milliseconds);
    }

    const uint32_t millihz = timer_frequency_millihz();

    printf(
        "  %llu ticks at %lu.%03lu Hz\n",
        (uint64_t) timer_ticks,
        (unsigned long) (millihz / 1000u),
        (unsigned long) (millihz % 1000u));

    return 0;
}

} // namespace blockos::cmd
