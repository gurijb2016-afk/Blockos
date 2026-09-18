#define _GNU_SOURCE
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

static char *const envp[] = {
    (char *)"DISPLAY=:0",
    (char *)"XDG_SESSION_TYPE=x11",
    (char *)"XDG_CURRENT_DESKTOP=GNOME",
    (char *)"DESKTOP_SESSION=gnome-blockos",
    (char *)"GDMSESSION=gnome-blockos",
    (char *)"GDK_BACKEND=x11",
    (char *)"CLUTTER_BACKEND=x11",
    (char *)"GSETTINGS_SCHEMA_DIR=/System/share/glib-2.0/schemas",
    (char *)"DCONF_PROFILE=user",
    (char *)"PATH=/System/bin:/bin",
    (char *)"HOME=/home/blockos",
    NULL
};

static pid_t spawn(char *const argv[]) {
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        execve(argv[0], argv, envp);
        _exit(127);
    }
    return pid;
}

int main(void) {
    char *dbus[] = { (char *)"/System/bin/dbusd", (char *)"--session", NULL };
    char *x11[]  = { (char *)"/System/bin/Xorg", (char *)":0", NULL };
    char *session[] = { (char *)"/System/bin/gnome-session", (char *)"--session=gnome-blockos", NULL };

    pid_t dbus_pid = spawn(dbus);
    if (dbus_pid < 0) return 10;

    pid_t x_pid = spawn(x11);
    if (x_pid < 0) return 11;

    /* Give the X server a chance to create the display socket. */
    usleep(250000);

    pid_t session_pid = spawn(session);
    if (session_pid < 0) return 12;

    int status = 0;
    while (waitpid(session_pid, &status, 0) < 0) {
        if (errno != EINTR) break;
    }

    kill(x_pid, SIGTERM);
    kill(dbus_pid, SIGTERM);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}
