/* Client half of the x11_sock test. Started by x11_sock via fork + execv,
 * connects to /tmp/x11test, sends "ping", expects "pong", exits 0 on success. */
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>

static void pause_ms(long ms) {
    struct timespec ts = { 0, ms * 1000000L };
    nanosleep(&ts, NULL);
}

static int read_full(int fd, char* buf, int want) {
    int got = 0;
    for (int tries = 0; got < want && tries < 5000; tries++) {
        int n = (int)read(fd, buf + got, (size_t)(want - got));
        if (n > 0) got += n;
        else if (n == 0) break;
        else pause_ms(1);
    }
    return got;
}

int main(void) {
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof addr);
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, "/tmp/x11test");

    int c = socket(AF_UNIX, SOCK_STREAM, 0);
    if (c < 0) _exit(10);
    int connected = -1;
    for (int tries = 0; tries < 5000 && connected < 0; tries++) {
        connected = connect(c, (struct sockaddr*)&addr, sizeof addr);
        if (connected < 0) pause_ms(1);
    }
    if (connected < 0) _exit(11);
    if (write(c, "ping", 4) != 4) _exit(12);
    char buf[8];
    if (read_full(c, buf, 4) != 4 || memcmp(buf, "pong", 4) != 0) _exit(13);
    _exit(0);
}
