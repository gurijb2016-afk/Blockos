/* Socket / fork / poll smoke test for BlockOS libc + kernel.
 * Covers AF_UNIX stream sockets (socket, bind, listen, accept, connect,
 * read, write), getsockname/getpeername, poll, and fork + wait4.
 * Built static against libc.a; run from the BlockOS shell as `x11_sock`. */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>

static int failures = 0;

/* File scope on purpose: the fork child shares this stack until it execs, so
 * it must not write into main()'s frame (that would clobber the parent's
 * locals, e.g. the fork result). */
static char* const child_argv[] = { (char*)"x11_child", 0 };

#define CHECK(cond, what)                                             \
    do {                                                              \
        if (cond) printf("PASS %s\n", what);                          \
        else { printf("FAIL %s (errno=%d)\n", what, errno); failures++; } \
    } while (0)

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

    int srv = socket(AF_UNIX, SOCK_STREAM, 0);
    CHECK(srv >= 0, "socket(AF_UNIX, SOCK_STREAM)");
    CHECK(bind(srv, (struct sockaddr*)&addr, sizeof addr) == 0, "bind");
    CHECK(listen(srv, 4) == 0, "listen");

    struct sockaddr_un got;
    unsigned int glen = sizeof got;
    memset(&got, 0, sizeof got);
    CHECK(getsockname(srv, (struct sockaddr*)&got, &glen) == 0 &&
          strcmp(got.sun_path, "/tmp/x11test") == 0, "getsockname returns bound path");

    int pid = fork();
    if (pid < 0) {
        CHECK(0, "fork");
        printf("X11SOCK FAILURES=%d\n", failures + 1);
        return 1;
    }
    if (pid == 0) {
        /* BlockOS fork is vfork-like (shared address space until exec), so
         * the child must exec before doing any work. The client side lives
         * in /bin/x11_child. */
        execv("/bin/x11_child", child_argv);
        _exit(120);
    }

    /* parent: server side. Wait for the connection with poll, then accept. */
    int cfd = -1;
    for (int tries = 0; tries < 5000 && cfd < 0; tries++) {
        struct pollfd pf = { srv, POLLIN, 0 };
        poll(&pf, 1, 10);
        cfd = accept(srv, NULL, NULL);
        if (cfd < 0) pause_ms(1);
    }
    CHECK(cfd >= 0, "accept (after poll POLLIN)");

    if (cfd >= 0) {
        char buf[8];
        CHECK(read_full(cfd, buf, 4) == 4 && memcmp(buf, "ping", 4) == 0, "server read \"ping\"");
        CHECK(write(cfd, "pong", 4) == 4, "server write \"pong\"");
        struct sockaddr_un peer;
        unsigned int plen = sizeof peer;
        memset(&peer, 0, sizeof peer);
        CHECK(getpeername(cfd, (struct sockaddr*)&peer, &plen) == 0, "getpeername on accepted socket");
        close(cfd);
    }

    int status = -1;
    int r = wait4(pid, &status, 0, NULL);
    CHECK(r == pid, "wait4 returns child pid");
    CHECK(r == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0, "child exited with status 0");

    close(srv);
    if (failures == 0) printf("X11SOCK ALL PASS\n");
    printf("X11SOCK FAILURES=%d\n", failures);
    return failures ? 1 : 0;
}
