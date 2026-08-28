#include <fcntl.h>
#include <unistd.h>
int main(void){int fd=open("/blockos/example.txt",O_CREAT|O_RDWR,0644);if(fd<0)return 1;const char s[]="BlockOS file example\n";write(fd,s,sizeof(s)-1);fsync(fd);close(fd);return 0;}
