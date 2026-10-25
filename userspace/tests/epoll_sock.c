/* epoll on a listening unix socket: the path X uses (ospoll + epoll). */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>

static int fails;
#define CHECK(c, msg) do { if (c) printf("PASS %s\n", msg); else { printf("FAIL %s (errno %d)\n", msg, errno); fails++; } } while (0)

int main(void) {
    struct sockaddr_un sa;
    memset(&sa, 0, sizeof sa);
    sa.sun_family = AF_UNIX;
    strcpy(sa.sun_path, "/tmp/epoll_test.sock");
    unlink(sa.sun_path);

    int ls = socket(AF_UNIX, SOCK_STREAM, 0);
    CHECK(ls >= 0, "listen socket");
    CHECK(bind(ls, (struct sockaddr *) &sa, sizeof sa) == 0, "bind");
    CHECK(listen(ls, 4) == 0, "listen");

    int ep = epoll_create1(0);
    CHECK(ep >= 0, "epoll_create1");
    struct epoll_event ev;
    memset(&ev, 0, sizeof ev);
    ev.events = EPOLLIN;
    ev.data.fd = ls;
    CHECK(epoll_ctl(ep, EPOLL_CTL_ADD, ls, &ev) == 0, "epoll_ctl ADD listener");

    struct epoll_event out[4];
    CHECK(epoll_wait(ep, out, 4, 0) == 0, "epoll_wait with no client is empty");

    int cs = socket(AF_UNIX, SOCK_STREAM, 0);
    CHECK(connect(cs, (struct sockaddr *) &sa, sizeof sa) == 0, "client connect");

    int n = epoll_wait(ep, out, 4, 1000);
    CHECK(n == 1 && (out[0].events & EPOLLIN), "epoll_wait reports listener readable after connect");

    close(cs);
    close(ls);
    close(ep);
    printf("EPOLLSOCK FAILURES=%d\n", fails);
    return fails ? 1 : 0;
}
