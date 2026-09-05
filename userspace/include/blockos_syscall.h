#pragma once
#include <stdint.h>
static inline long blockos_syscall6(long n,long a0,long a1,long a2,long a3,long a4,long a5){long r;__asm__ volatile("int $0x80":"=a"(r):"a"(n),"D"(a0),"S"(a1),"d"(a2),"r"(a3),"r"(a4),"r"(a5):"memory","cc");return r;}
