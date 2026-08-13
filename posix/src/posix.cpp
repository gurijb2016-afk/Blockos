#include "../include/unistd.h"
#include "../include/fcntl.h"
#include "../include/errno.h"
#include "../include/sys/stat.h"
#include "../include/sys/wait.h"
#include "../include/signal.h"
#include "../include/sys/mman.h"
#include "../include/internal/syscall.h"

thread_local int errno = 0;
char** environ = nullptr;

using blockos::abi::Dispatcher;
using blockos::abi::Number;

extern "C" ssize_t read(int fd, void* b, size_t n) { return Dispatcher::call(Number::Read,fd,(uintptr_t)b,n); }
extern "C" ssize_t write(int fd, const void* b, size_t n) { return Dispatcher::call(Number::Write,fd,(uintptr_t)b,n); }
extern "C" int close(int fd) { return (int)Dispatcher::call(Number::Close,fd); }
extern "C" int open(const char* p,int f,mode_t m) { return (int)Dispatcher::call(Number::Open,(uintptr_t)p,f,m); }
extern "C" int fcntl(int fd,int cmd,long arg) { return (int)Dispatcher::call(Number::Fcntl,fd,cmd,arg); }
extern "C" int pipe(int f[2]) { return (int)Dispatcher::call(Number::Pipe,(uintptr_t)f); }
extern "C" int dup(int fd) { return (int)Dispatcher::call(Number::Dup,fd); }
extern "C" int dup2(int a,int b) { return (int)Dispatcher::call(Number::Dup2,a,b); }
extern "C" pid_t fork() { return (pid_t)Dispatcher::call(Number::Fork); }
extern "C" int execve(const char* p,char* const a[],char* const e[]) { return (int)Dispatcher::call(Number::Execve,(uintptr_t)p,(uintptr_t)a,(uintptr_t)e); }
extern "C" int execv(const char* p,char* const a[]) { return execve(p,a,environ); }
extern "C" pid_t waitpid(pid_t p,int* s,int o) { return (pid_t)Dispatcher::call(Number::Waitpid,p,(uintptr_t)s,o); }
extern "C" pid_t getpid() { return (pid_t)Dispatcher::call(Number::Getpid); }
extern "C" pid_t getppid() { return (pid_t)Dispatcher::call(Number::Getppid); }
extern "C" int chdir(const char* p) { return (int)Dispatcher::call(Number::Chdir,(uintptr_t)p); }
extern "C" char* getcwd(char* b,size_t n) { return Dispatcher::call(Number::Getcwd,(uintptr_t)b,n) < 0 ? nullptr : b; }
extern "C" int stat(const char* p,struct stat* s) { return (int)Dispatcher::call(Number::Stat,(uintptr_t)p,(uintptr_t)s); }
extern "C" int fstat(int f,struct stat* s) { return (int)Dispatcher::call(Number::Fstat,f,(uintptr_t)s); }
extern "C" int mkdir(const char* p,mode_t m) { return (int)Dispatcher::call(Number::Mkdir,(uintptr_t)p,m); }
extern "C" int unlink(const char* p) { return (int)Dispatcher::call(Number::Unlink,(uintptr_t)p); }
extern "C" int rename(const char* a,const char* b) { return (int)Dispatcher::call(Number::Rename,(uintptr_t)a,(uintptr_t)b); }
extern "C" int kill(pid_t p,int s) { return (int)Dispatcher::call(Number::Kill,p,s); }
extern "C" sighandler_t signal(int s,sighandler_t h) { return (sighandler_t)Dispatcher::call(Number::Signal,s,(uintptr_t)h); }
extern "C" unsigned sleep(unsigned s) { return (unsigned)Dispatcher::call(Number::Sleep,s); }
extern "C" unsigned alarm(unsigned s) { return (unsigned)Dispatcher::call(Number::Sleep,s); }
extern "C" void* mmap(void* a,size_t l,int p,int f,int fd,off_t o) { return (void*)(uintptr_t)Dispatcher::call(Number::Mmap,(uintptr_t)a,l,p,(uint64_t)f,fd,o); }
extern "C" int munmap(void* a,size_t l) { return (int)Dispatcher::call(Number::Munmap,(uintptr_t)a,l); }
extern "C" int brk(void* e) { return (int)Dispatcher::call(Number::Brk,(uintptr_t)e); }
extern "C" int setenv(const char*,const char*,int) { errno=ENOSYS; return -1; }
extern "C" int unsetenv(const char*) { errno=ENOSYS; return -1; }
extern "C" const char* getenv(const char*) { errno=ENOSYS; return nullptr; }
extern "C" [[noreturn]] void _exit(int s) { (void)Dispatcher::call(Number::Exit,(uint64_t)s); for(;;) { asm volatile("hlt"); } }
