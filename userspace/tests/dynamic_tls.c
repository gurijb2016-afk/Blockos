#include <stdio.h>
#include <errno.h>
#include <pthread.h>

static _Thread_local int tls_value;

static void* worker(void* arg) {
    (void)arg;
    tls_value = 42;
    errno = 7;
    return (void*)0x1234;
}

int main(void) {
    pthread_t t;
    tls_value = 11;
    if (pthread_create(&t, 0, worker, 0) != 0) return 2;
    void* ret = 0;
    if (pthread_join(t, &ret) != 0) return 3;
    printf("dynamic-tls main=%d errno=%d worker=%p\n", tls_value, errno, ret);
    return (tls_value == 11 && ret == (void*)0x1234) ? 0 : 4;
}
