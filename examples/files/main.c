#include <fcntl.h>
#include <unistd.h>

int main(void)
{
    int fd = open("/blockos/example.txt", O_CREAT | O_RDWR, 0644);
    if (fd < 0)
        return 1;

    static const char text[] = "BlockOS file example\n";
    if (write(fd, text, sizeof(text) - 1) < 0) {
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}
