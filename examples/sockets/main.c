#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

int main(void)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return 1;
    close(fd);
    return 0;
}
