/* Prints what the process was started with: argc, argv, environ, and getenv().
 * Used to check the static-binary startup path (crt1 -> __blockos_set_environ). */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
    printf("argc=%d\n", argc);
    for (int i = 0; i < argc; i++) printf("argv[%d]=%s\n", i, argv[i] ? argv[i] : "(null)");
    printf("environ=%p\n", (void *)environ);
    if (environ) for (char **e = environ; *e; e++) printf("env: %s\n", *e);
    char *h = getenv("HOME");
    printf("HOME=%s\n", h ? h : "(unset)");
    return 0;
}
