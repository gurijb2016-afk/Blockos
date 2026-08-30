#include <pthread.h>
#include <unistd.h>

static void *worker(void *arg)
{
    (void)arg;
    static const char msg[] = "BlockOS worker thread\n";
    write(1, msg, sizeof(msg) - 1);
    return 0;
}

int main(void)
{
    pthread_t t;
    if (pthread_create(&t, 0, worker, 0) != 0)
        return 1;
    return pthread_join(t, 0) != 0;
}
