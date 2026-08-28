#include <unistd.h>
int main(void){ const char s[]="Hello from BlockOS POSIX!\n"; return write(1,s,sizeof(s)-1)<0; }
