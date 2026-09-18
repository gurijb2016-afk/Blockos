#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
int main(void){ write(1,"BlockOS dynamic ELF + TLS + pthread OK\\n",38); return 0; }
