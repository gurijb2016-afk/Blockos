#include "init/logger.hpp"

#include <starg.h>
#include <stdio.h>

namespace blockos::init {

void log(LogLevel level, const char* format, ...)
{
    const char* prefix = "[INFO] ";

    switch (level)
    {
        case LogLevel::Info:
            prefix = "[INFO] ";
            break;
        case LogLevel::Warning:
            prefix = "[WARN] ";
            break;
        case LogLevel::Error:
            prefix = "[ERROR] ";
            break;
    }

    fputs(prefix, stdout);

    va_list args;
    va_start(args, format);
    vfprintf(stdout, format, args);
    va_end(args);

    fputc('\n', stdout);
}

} // namespace blockos::ninit
