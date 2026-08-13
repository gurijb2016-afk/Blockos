#pragma once
#include <stddef.h>
#include <sys/types.h>
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#ifdef __cplusplus
extern "C" {
#endif
extern char** environ;
ssize_t read(int fd, void* buf, size_t count);
ssize_t write(int fd, const void* buf, size_t count);
int close(int fd);
pid_t fork(void);
int execve(const char* path, char* const argv[], char* const envp[]);
int execv(const char* path, char* const argv[]);
pid_t getpid(void);
pid_t getppid(void);
pid_t waitpid(pid_t pid, int* status, int options);
int pipe(int fds[2]);
int dup(int fd);
int dup2(int oldfd, int newfd);
unsigned sleep(unsigned seconds);
int chdir(const char* path);
char* getcwd(char* buf, size_t size);
int kill(pid_t pid, int sig);
int setenv(const char* name, const char* value, int overwrite);
int unsetenv(const char* name);
const char* getenv(const char* name);
[[noreturn]] void _exit(int status);
#ifdef __cplusplus
}
#endif
