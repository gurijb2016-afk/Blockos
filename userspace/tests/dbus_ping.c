#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include "../services/dbus_protocol.h"

static uint32_t rd32(const uint8_t *p){uint32_t v;memcpy(&v,p,4);return v;}
static void wr32(uint8_t*p,uint32_t v){memcpy(p,&v,4);}
static size_t a8(size_t n){return(n+7)&~(size_t)7;}
static size_t hf(uint8_t*d,size_t p,uint8_t c,char t){p=a8(p);d[p++]=c;d[p++]=2;d[p++]=(uint8_t)t;d[p++]=0;return a8(p);}
static size_t hs(uint8_t*d,size_t p,uint8_t c,char t,const char*s){uint32_t n=(uint32_t)strlen(s);p=hf(d,p,c,t);wr32(d+p,n);p+=4;memcpy(d+p,s,n);p+=n;d[p++]=0;return p;}
static size_t body_s(uint8_t*d,size_t p,const char*s){uint32_t n=(uint32_t)strlen(s);p=(p+3)&~(size_t)3;wr32(d+p,n);p+=4;memcpy(d+p,s,n);p+=n;d[p++]=0;return p;}
static size_t hdr(uint8_t*m,uint8_t type,uint32_t serial,const char*path,const char*iface,const char*member,const char*sig){size_t p=16;m[0]='l';m[1]=type;m[2]=0;m[3]=1;wr32(m+8,serial);if(path)p=hs(m,p,1,'o',path);if(iface)p=hs(m,p,2,'s',iface);if(member)p=hs(m,p,3,'s',member);if(sig)p=hs(m,p,8,'g',sig);p=a8(p);wr32(m+12,(uint32_t)(p-16));return p;}
int main(void){int fd=socket(AF_UNIX,SOCK_STREAM,0);struct sockaddr_un a;uint8_t m[1024],r[4096],body[256];if(fd<0)return 1;memset(&a,0,sizeof(a));a.sun_family=AF_UNIX;strcpy(a.sun_path,"/run/dbus/system_bus_socket");if(connect(fd,(struct sockaddr*)&a,2+strlen(a.sun_path)+1)<0)return 2;write(fd,"\0AUTH EXTERNAL 30\r\n",20);read(fd,r,sizeof(r));write(fd,"BEGIN\r\n",7);
size_t p=hdr(m,DBUS_MESSAGE_METHOD_CALL,1,"/org/freedesktop/DBus","org.freedesktop.DBus","Hello","");wr32(m+4,0);write(fd,m,p);int n=(int)read(fd,r,sizeof(r));if(n<16||r[1]!=DBUS_MESSAGE_METHOD_RETURN)return 3;
printf("dbus Hello reply received (%d bytes)\n",n);close(fd);return 0;}
