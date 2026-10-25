/* Mimics Xtrans' unix listener setup (mkdir, socket, bind, listen) and prints
 * errno at each step, to find why the X server's bind fails on BlockOS. */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

int main(void) {
    int r = mkdir("/tmp/.X11-unix", 0777);
    printf("mkdir rc=%d errno=%d\n", r, errno);
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    printf("socket fd=%d errno=%d\n", fd, errno);
    struct sockaddr_un sa;
    memset(&sa, 0, sizeof sa);
    sa.sun_family = AF_UNIX;
    strcpy(sa.sun_path, "/tmp/.X11-unix/X0");
    unlink("/tmp/.X11-unix/X0");
    for (unsigned n = sizeof(sa.sun_family) + strlen(sa.sun_path) + 1; n <= sizeof sa; n += (sizeof sa - n)) {
        r = bind(fd, (struct sockaddr *)&sa, n);
        printf("bind len=%u rc=%d errno=%d\n", n, r, errno);
        break;
    }
    r = bind(fd, (struct sockaddr *)&sa, sizeof sa);
    printf("bind full len=%zu rc=%d errno=%d\n", sizeof sa, r, errno);
    printf("listen rc=%d errno=%d\n", listen(fd, 4), errno);
    return 0;
}
