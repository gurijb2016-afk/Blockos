#pragma once
#include <sys/types.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/time.h>

typedef long ssize_t;
typedef long off_t;
typedef int  pid_t;

struct iovec;

ssize_t read(int fd, void* buf, size_t count);
ssize_t write(int fd, const void* buf, size_t count);
int     close(int fd);
off_t   lseek(int fd, off_t offset, int whence);
pid_t   getpid(void);
pid_t   getppid(void);
pid_t   fork(void);
pid_t   vfork(void);
int     setpgid(pid_t pid, pid_t pgid);
pid_t   getpgid(pid_t pid);
pid_t   getpgrp(void);
pid_t   setsid(void);
void*   sbrk(long increment);   /* returns (void*)-1 on failure */
__attribute__((noreturn)) void _exit(int code);

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

int open(const char* path, int flags, ...);
int openat(int dirfd, const char* path, int flags, ...);

int sched_yield(void);
int nanosleep(const struct timespec*, struct timespec*);
unsigned sleep(unsigned);
int usleep(unsigned);

int unlink(const char* path);
int rename(const char* oldpath, const char* newpath);
int mkdir(const char* path, unsigned mode);
int execv(const char* path, char* const argv[]);
int execvp(const char* file, char* const argv[]);
ssize_t readlink(const char* path, char* buf, size_t size);
ssize_t readlinkat(int dirfd, const char* path, char* buf, size_t size);

int dup(int fd);
int dup2(int oldfd, int newfd);
int isatty(int fd);
ssize_t readv(int fd, const struct iovec* iov, int iovcnt);
ssize_t writev(int fd, const struct iovec* iov, int iovcnt);

int access(const char* path, int mode);
int rmdir(const char* path);
int ftruncate(int fd, off_t size);
int fsync(int fd);
long sysconf(int name);
long getpagesize(void);
#define _SC_PAGESIZE 2
#define _SC_OPEN_MAX 4

#ifndef F_OK
#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4
#endif

int pipe(int fds[2]);
int link(const char *oldpath, const char *newpath);
char *basename(char *path);
char *dirname(char *path);
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
uid_t getuid(void);
uid_t geteuid(void);
gid_t getgid(void);
gid_t getegid(void);
int gethostname(char *name, size_t len);
int setuid(uid_t uid);
int setgid(gid_t gid);
int seteuid(uid_t uid);
int setegid(gid_t gid);
int execl(const char *path, const char *arg, ...);

#ifndef BLOCKOS_UNISTD_X11_COMPAT
#define BLOCKOS_UNISTD_X11_COMPAT
int getresuid(unsigned int *ruid, unsigned int *euid, unsigned int *suid);
int getresgid(unsigned int *rgid, unsigned int *egid, unsigned int *sgid);
#endif

int execlp(const char *file, const char *arg, ...);
