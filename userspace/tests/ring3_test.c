#include <stdint.h>
static long sc(long n,long a0,long a1,long a2,long a3,long a4,long a5){long r;__asm__ volatile("int $0x80":"=a"(r):"a"(n),"D"(a0),"S"(a1),"d"(a2),"r"(a3),"r"(a4),"r"(a5):"rcx","r11","memory","cc");return r;}
static const char msg[]="BlockOS ring3 OK\n";
int main(void){sc(1,1,(long)msg,sizeof(msg)-1,0,0,0);sc(44,0,0,0,0,0,0);return 0;}
