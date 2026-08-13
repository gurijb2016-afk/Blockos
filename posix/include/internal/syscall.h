#pragma once
#include <stdint.h>
#include <stddef.h>
namespace blockos::abi {
enum class Number : uint64_t {
    Read=0, Write=1, Open=2, Close=3, Fstat=4, Stat=5, Lseek=6,
    Pipe=7, Dup=8, Dup2=9, Fcntl=10, Mkdir=11, Unlink=12, Rename=13,
    Chdir=14, Getcwd=15, Fork=20, Execve=21, Waitpid=22, Getpid=23,
    Getppid=24, Exit=25, Kill=26, Signal=27, Sleep=28, Mmap=30,
    Munmap=31, Brk=32, Setenv=40, Unsetenv=41, Getenv=42
};
struct Result { int64_t value; };
struct Dispatcher {
    static int64_t call(Number n, uint64_t a0=0, uint64_t a1=0, uint64_t a2=0,
                         uint64_t a3=0, uint64_t a4=0, uint64_t a5=0);
};
}
