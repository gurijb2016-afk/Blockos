#pragma once
#include <stdint.h>
#include <sys/time.h>
struct itimerval { struct timeval it_interval; struct timeval it_value; };
struct rusage { struct timeval ru_utime; struct timeval ru_stime; long ru_maxrss; };
struct rlimit { uint64_t rlim_cur; uint64_t rlim_max; };
#define RUSAGE_SELF 0
#define RUSAGE_CHILDREN (-1)
#define ITIMER_REAL 0
int getrusage(int who, struct rusage *usage);
int setitimer(int, const struct itimerval*, struct itimerval*);
int getrlimit(int resource, struct rlimit* r);
int setrlimit(int resource, const struct rlimit* r);
#define RLIMIT_CORE 4
#define RLIMIT_NOFILE 7
