#include <unistd.h>

int main(void)
{
    static const char msg[] = "Hello from BlockOS!\n";
    return write(1, msg, sizeof(msg) - 1) < 0;
}
