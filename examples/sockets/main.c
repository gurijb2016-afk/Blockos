#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
int main(void){int s=socket(AF_INET,SOCK_STREAM,0); if(s<0)return 1; close(s); return 0;}
