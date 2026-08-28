#pragma once
#include <stddef.h>
#include <sys/types.h>
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4
#ifdef __cplusplus
extern "C" {
#endif
ssize_t read(int,const void*,size_t); ssize_t write(int,const void*,size_t); int close(int);
int open(const char*,int,...); int openat(int,const char*,int,...); off_t lseek(int,off_t,int);
ssize_t readv(int,const void*,int); ssize_t writev(int,const void*,int);
pid_t fork(void); int execve(const char*,char* const[],char* const[]); pid_t waitpid(pid_t,int*,int);
pid_t getpid(void); pid_t getppid(void); pid_t gettid(void); int chdir(const char*); char* getcwd(char*,size_t);
int access(const char*,int); ssize_t readlink(const char*,char*,size_t); int pipe(int[2]); int pipe2(int[2],int);
int dup(int); int dup2(int,int); unsigned sleep(unsigned); int usleep(unsigned); int sched_yield(void);
uid_t getuid(void); uid_t geteuid(void); gid_t getgid(void); gid_t getegid(void);
int setuid(uid_t); int seteuid(uid_t); int setgid(gid_t); int setegid(gid_t);
int ftruncate(int,off_t); int fsync(int); int ioctl(int,unsigned long,...);
int kill(pid_t,int); void _exit(int) __attribute__((noreturn));
#ifdef __cplusplus
}
#endif
