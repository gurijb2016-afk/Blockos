#include "../include/unistd.h"
#include "../include/fcntl.h"
#include "../include/sys/stat.h"
#include "../include/sys/wait.h"
int main() {
    const char msg[] = "BlockOS POSIX C++\n";
    write(STDOUT_FILENO,msg,sizeof(msg)-1);
    int fd=open("/blockos/test",O_CREAT|O_RDWR,0644);
    if(fd>=0){ write(fd,msg,sizeof(msg)-1); close(fd); }
    int fds[2]; pipe(fds); close(fds[0]); close(fds[1]);
    return 0;
}
