#include "socket.hpp"
#include "udp.hpp"
#include "tcp.hpp"
#include <string.h>

namespace blockos::net {

struct Socket {
    bool used;
    int type;
    int proto;
    int transport;
    uint16_t local_port;
};

static Socket sockets[32]{};

int socket(int domain, int type, int protocol) {
    if (domain != 2 || (type != SOCK_STREAM && type != SOCK_DGRAM)) return -1;
    for (int i=0;i<32;i++) if (!sockets[i].used) {
        sockets[i]={true,type,protocol,-1,0};
        return i;
    }
    return -1;
}

int bind(int fd,uint16_t port) {
    if(fd<0||fd>=32||!sockets[fd].used||!port) return -1;
    sockets[fd].local_port=port;
    if(sockets[fd].type==SOCK_STREAM)
        return tcp_listen(port)?0:-1;
    return 0;
}

int connect(int fd,const IPv4Address& addr,uint16_t port) {
    if(fd<0||fd>=32||!sockets[fd].used) return -1;
    if(sockets[fd].type!=SOCK_STREAM) return -1;
    int h=tcp_connect(addr,port);
    if(h<0) return -1;
    sockets[fd].transport=h;
    return 0;
}

int listen(int fd,int) {
    if(fd<0||fd>=32||!sockets[fd].used||sockets[fd].type!=SOCK_STREAM) return -1;
    return sockets[fd].local_port ? 0 : -1;
}

int accept(int fd) {
    if(fd<0||fd>=32||!sockets[fd].used||sockets[fd].type!=SOCK_STREAM||!sockets[fd].local_port)
        return -1;
    return -2; // EAGAIN: no pending TCP connection
}

int send(int fd,const void* buf,size_t len) {
    if(fd<0||fd>=32||!sockets[fd].used||!buf) return -1;
    if(sockets[fd].type==SOCK_STREAM)
        return tcp_send(sockets[fd].transport,buf,len);
    return -1;
}

int recv(int fd,void* buf,size_t len) {
    if(fd<0||fd>=32||!sockets[fd].used||!buf) return -1;
    if(sockets[fd].type==SOCK_STREAM)
        return tcp_receive(sockets[fd].transport,buf,len);
    return -1;
}

int close(int fd) {
    if(fd<0||fd>=32||!sockets[fd].used) return -1;
    if(sockets[fd].type==SOCK_STREAM && sockets[fd].transport>=0)
        tcp_close(sockets[fd].transport);
    sockets[fd]={};
    return 0;
}

} // namespace blockos::net
