#include <pthread.h>
#include <unistd.h>
static void* worker(void*arg){const char s[]="BlockOS thread\n";write(1,s,sizeof(s)-1);return arg;}
int main(void){pthread_t t; if(pthread_create(&t,0,worker,0)!=0)return 1; pthread_join(t,0); return 0;}
