#pragma once

namespace blockos::init {

int process_spawn(
    const char* path,
    const char* const argv[],
    const char* const envp[]
);

bool process_alive(int pid);

int process_signal(
    int pid,
    int signal_number
);

} // namespace blockos::init
