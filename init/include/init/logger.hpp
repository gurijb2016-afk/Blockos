#pragma once

namespace blockos::init {

enum class LogLevel
{
    Info,
    Warning,
    Error
};

void log(
    LogLevel level,
    const char* format,
    ...
);

} // namespace blockos::init
