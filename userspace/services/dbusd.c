#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include "sys/socket.h"
#include "sys/poll.h"

#define MAX_CLIENTS 32
#define NAME_LEN 128
#define PATH_LEN 108
#define BUS_PATH "/run/dbus/system_bus_socket"
#define SESSION_BUS_PATH "/run/user/0/bus"

typedef struct {
    int fd;
    char unique[NAME_LEN];
    char names[8][NAME_LEN];
    int name_count;
    uint32_t serial;
    int greeted;
    int authenticated;
    uint8_t authbuf[1024];
    size_t authlen;
    uint8_t inbuf[65536];
    size_t inlen;
} Client;

static Client clients[MAX_CLIENTS];
static uint32_t next_unique = 1;
static char g_bus_path[PATH_LEN] = BUS_PATH;
static const char g_bus_guid[] = "424c4f434b4f53340000000000000001";

static void put_u32(uint8_t *p,uint32_t v){memcpy(p,&v,4);}
static uint32_t get_u32(const uint8_t*p){uint32_t v;memcpy(&v,p,4);return v;}
static size_t align8(size_t x){return (x+7)&~(size_t)7;}

struct MsgHdr { char endian; uint8_t type, flags, version; uint32_t body_len, serial, fields_len; };

/* D-Bus header field parser: field array entries are variants. We only need
 * STRING/OBJECT_PATH fields and UINT32 for reply serial while bootstrapping. */
static int field_strings(const uint8_t *buf,size_t len,uint8_t wanted,char *out,size_t cap){
    size_t pos=0;
    while(pos+4<=len){
        uint8_t code=buf[pos]; const uint8_t *q=buf+pos+1;
        size_t n=q-buf;
        if(n>=len)break;
        uint8_t siglen=buf[n++]; if(siglen==0||n+siglen>len)break;
        const char* sig=(const char*)(buf+n); n+=siglen;
        n=align8(n);
        size_t str_n=0;
        if(siglen>=2 && sig[0]=='s' && sig[1]==0){
            if(n+4>len)break; uint32_t l=get_u32(buf+n); n+=4; if(n+l+1>len)break; str_n=l<cap-1?l:cap-1; if(cap) {memcpy(out,buf+n,str_n);out[str_n]=0;}
            if(code==wanted)return 1;
            n+=l+1;
        } else if(siglen>=2 && sig[0]=='o' && sig[1]==0){
            if(n+4>len)break; uint32_t l=get_u32(buf+n); n+=4; if(n+l+1>len)break; str_n=l<cap-1?l:cap-1; if(cap){memcpy(out,buf+n,str_n);out[str_n]=0;} if(code==wanted)return 1; n+=l+1;
        } else if(siglen>=2 && sig[0]=='u' && sig[1]==0){
            if(n+4>len)break; n+=4;
        } else {
            break;
        }
        pos=align8(n);
    }
    return 0;
}

static uint32_t field_u32(const uint8_t *buf,size_t len,uint8_t wanted){
    size_t pos=0;
    while(pos+4<=len){
        uint8_t code=buf[pos]; size_t n=pos+1; if(n>=len)break;
        uint8_t siglen=buf[n++];if(!siglen||n+siglen>len)break;const char*sig=(const char*)(buf+n);n+=siglen;n=align8(n);
        if(siglen>=2&&sig[0]=='u'&&sig[1]==0){if(n+4>len)break;uint32_t v=get_u32(buf+n);if(code==wanted)return v;n+=4;}
        else if(siglen>=2&&sig[0]=='s'&&sig[1]==0){if(n+4>len)break;uint32_t l=get_u32(buf+n);n+=4+l+1;}
        else break;
        pos=align8(n);
    }
    return 0;
}

static size_t add_string_field(uint8_t *dst,size_t pos,uint8_t code,const char*sig,const char*value){
    pos=align8(pos);dst[pos++]=code;size_t sl=strlen(sig)+1;dst[pos++]=(uint8_t)sl;memcpy(dst+pos,sig,sl);pos+=sl;while(pos%8)dst[pos++]=0;uint32_t l=(uint32_t)strlen(value);memcpy(dst+pos,&l,4);pos+=4;memcpy(dst+pos,value,l);pos+=l;dst[pos++]=0;return pos;
}
static size_t add_u32_field(uint8_t *dst,size_t pos,uint8_t code,uint32_t value){
    pos=align8(pos);dst[pos++]=code;dst[pos++]=2;dst[pos++]='u';dst[pos++]=0;while(pos%8)dst[pos++]=0;memcpy(dst+pos,&value,4);return pos+4;
}

/* Sends a contiguous simple D-Bus method return with optional STRING body. */
static int send_reply(Client*c,uint32_t reply_serial,const char*body,uint32_t body_u32,int use_u32){
    uint8_t msg[2048];memset(msg,0,sizeof(msg));
    msg[0]='l';msg[1]=2;msg[2]=0;msg[3]=1;uint32_t serial=++c->serial;put_u32(msg+8,serial);
    size_t pos=16;pos=add_u32_field(msg,pos,5,reply_serial);if(c->greeted)pos=add_string_field(msg,pos,6,"s",c->unique);pos=align8(pos);uint32_t field_len=(uint32_t)(pos-16);put_u32(msg+12,field_len);
    size_t body_start=pos;size_t body_len=0;
    if(body){uint32_t l=(uint32_t)strlen(body);put_u32(msg+pos,l);pos+=4;memcpy(msg+pos,body,l);pos+=l;msg[pos++]=0;body_len=4+l+1;}
    if(use_u32){put_u32(msg+pos,body_u32);pos+=4;body_len=4;}
    put_u32(msg+4,(uint32_t)body_len);
    return write(c->fd,msg,pos)<0?-1:0;
}

static void remove_client(int idx){if(clients[idx].fd>=0)close(clients[idx].fd);memset(&clients[idx],0,sizeof(clients[idx]));clients[idx].fd=-1;}
static int client_for_name(const char*name){for(int i=0;i<MAX_CLIENTS;i++)if(clients[i].fd>=0){if(strcmp(clients[i].unique,name)==0)return i;for(int j=0;j<clients[i].name_count;j++)if(strcmp(clients[i].names[j],name)==0)return i;}return -1;}


static int auth_flush(Client *c) {
    while (c->authlen) {
        size_t i;
        int found = 0;
        for (i = 0; i < c->authlen; ++i) if (c->authbuf[i] == '\r' || c->authbuf[i] == '\n') { found = 1; break; }
        if (!found) return 0;
        size_t line_len = i;
        while (line_len && (c->authbuf[line_len-1] == '\r' || c->authbuf[line_len-1] == '\n')) --line_len;
        char line[1024];
        size_t copy = line_len < sizeof(line)-1 ? line_len : sizeof(line)-1;
        memcpy(line, c->authbuf, copy); line[copy] = 0;
        size_t consume = i + 1;
        while (consume < c->authlen && (c->authbuf[consume] == '\r' || c->authbuf[consume] == '\n')) ++consume;
        size_t remain = c->authlen - consume;
        if (remain) memmove(c->authbuf, c->authbuf + consume, remain);
        c->authlen = remain;

        if (strcmp(line, "BEGIN") == 0) {
            c->authenticated = 1;
            continue;
        }
        if (strncmp(line, "AUTH EXTERNAL", 13) == 0) {
            write(c->fd, "OK " , 3); write(c->fd, g_bus_guid, sizeof(g_bus_guid)-1); write(c->fd, "\r\n", 2);
            continue;
        }
        if (strncmp(line, "NEGOTIATE_UNIX_FD", 17) == 0) {
            write(c->fd, "AGREE_UNIX_FD\r\n", 16);
            continue;
        }
        if (strncmp(line, "CANCEL", 6) == 0) {
            write(c->fd, "REJECTED\r\n", 11);
            continue;
        }
        write(c->fd, "REJECTED\r\n", 11);
    }
    return 1;
}

static int handle_raw_bytes(Client *c, const uint8_t *buf, size_t n) {
    size_t pos = 0;
    if (!c->authenticated) {
        /* AF_UNIX D-Bus authentication starts with a NUL byte. */
        if (c->authlen == 0 && n && buf[0] == 0) pos = 1;
        while (pos < n && !c->authenticated) {
            if (c->authlen >= sizeof(c->authbuf)) return 0;
            c->authbuf[c->authlen++] = buf[pos++];
            if (!auth_flush(c)) return 0;
        }
    }
    if (c->authenticated && pos < n) {
        if (c->inlen + (n-pos) > sizeof(c->inbuf)) return 0;
        memcpy(c->inbuf + c->inlen, buf + pos, n - pos);
        c->inlen += n - pos;
    }
    return 1;
}

static void handle_call(int ci,const uint8_t*msg,size_t n){
    if(n<16)return;Client*c=&clients[ci];uint8_t type=msg[1];uint32_t body_len=get_u32(msg+4),serial=get_u32(msg+8),fields_len=get_u32(msg+12);if(type!=1)return;size_t fields_off=16;if(fields_off+fields_len>n)return;const uint8_t*fields=msg+fields_off;size_t body_off=align8(fields_off+fields_len);if(body_off+body_len>n)return;
    char iface[NAME_LEN]={0},member[NAME_LEN]={0},dest[NAME_LEN]={0},path[NAME_LEN]={0};field_strings(fields,fields_len,2,iface,sizeof(iface));field_strings(fields,fields_len,3,member,sizeof(member));field_strings(fields,fields_len,6,dest,sizeof(dest));field_strings(fields,fields_len,1,path,sizeof(path));
    const uint8_t*b=msg+body_off;
    if(!c->greeted){if(strcmp(iface,"org.freedesktop.DBus")==0&&strcmp(member,"Hello")==0){snprintf(c->unique,sizeof(c->unique),":1.%lu",(unsigned long)next_unique++);c->greeted=1;send_reply(c,serial,c->unique,0,0);return;}return;}
    if(strcmp(dest,"org.freedesktop.DBus")==0||dest[0]==0){
        if(strcmp(member,"RequestName")==0){if(body_len<4)return;uint32_t l=get_u32(b);if(body_len<4+l+1||l>=NAME_LEN)return;char name[NAME_LEN];memcpy(name,b+4,l);name[l]=0;int owner=client_for_name(name);uint32_t reply=(owner<0)?1:4;if(owner<0&&c->name_count<8){strcpy(c->names[c->name_count++],name);}send_reply(c,serial,0,reply,1);return;}
        if(strcmp(member,"ReleaseName")==0){if(body_len<4)return;uint32_t l=get_u32(b);if(body_len<4+l+1||l>=NAME_LEN)return;char name[NAME_LEN];memcpy(name,b+4,l);name[l]=0;for(int j=0;j<c->name_count;j++)if(strcmp(c->names[j],name)==0){for(int k=j+1;k<c->name_count;k++)strcpy(c->names[k-1],c->names[k]);c->name_count--;break;}send_reply(c,serial,0,1,1);return;}
        if(strcmp(member,"NameHasOwner")==0){if(body_len<4)return;uint32_t l=get_u32(b);if(body_len<4+l+1||l>=NAME_LEN)return;char name[NAME_LEN];memcpy(name,b+4,l);name[l]=0;send_reply(c,serial,0,client_for_name(name)>=0?1:0,1);return;}
        if(strcmp(member,"GetNameOwner")==0){if(body_len<4)return;uint32_t l=get_u32(b);if(body_len<4+l+1||l>=NAME_LEN)return;char name[NAME_LEN];memcpy(name,b+4,l);name[l]=0;int oi=client_for_name(name);if(oi>=0){send_reply(c,serial,clients[oi].unique,0,0);}return;}
        return;
    }
    if(dest[0]){
        int oi=client_for_name(dest);if(oi>=0)write(clients[oi].fd,msg,n);return;
    }
    if(type==4){for(int i=0;i<MAX_CLIENTS;i++)if(i!=ci&&clients[i].fd>=0)write(clients[i].fd,msg,n);}
}


static int dispatch_buffered(Client *c){
    while(c->inlen>=16){
        if(c->inbuf[0]!='l' && c->inbuf[0]!='B') return -1;
        uint32_t body_len=get_u32(c->inbuf+4), fields_len=get_u32(c->inbuf+12);
        size_t body_off=align8(16+(size_t)fields_len);
        size_t total=body_off+(size_t)body_len;
        if(total>sizeof(c->inbuf)) return -1;
        if(c->inlen<total) return 0;
        handle_call((int)(c-clients),c->inbuf,total);
        size_t remain=c->inlen-total;
        if(remain) memmove(c->inbuf,c->inbuf+total,remain);
        c->inlen=remain;
    }
    return 0;
}

int main(int argc,char**argv){
    if(argc>1 && argv[1] && strcmp(argv[1],"--session")==0) strncpy(g_bus_path,SESSION_BUS_PATH,sizeof(g_bus_path)-1);
    int s=socket(AF_UNIX,SOCK_STREAM,0);if(s<0){printf("dbusd: socket failed\n");return 1;}
    unlink(g_bus_path);
    struct sockaddr_un a;memset(&a,0,sizeof(a));a.sun_family=AF_UNIX;strncpy(a.sun_path,g_bus_path,sizeof(a.sun_path)-1);
    if(bind(s,(struct sockaddr*)&a,2+strlen(a.sun_path)+1)<0){printf("dbusd: bind failed\n");return 1;}if(listen(s,32)<0)return 1;
    for(int i=0;i<MAX_CLIENTS;i++)clients[i].fd=-1;
    struct pollfd pfds[MAX_CLIENTS+1]; uint8_t buf[4096];
    for(;;){int pn=0;pfds[pn++] = (struct pollfd){s,POLLIN,0};for(int i=0;i<MAX_CLIENTS;i++)if(clients[i].fd>=0)pfds[pn++]=(struct pollfd){clients[i].fd,POLLIN,0};int pr=poll(pfds,pn,-1);if(pr<0)continue;if(pfds[0].revents&POLLIN){int cfd=accept(s,0,0);if(cfd>=0){int slot=-1;for(int i=0;i<MAX_CLIENTS;i++)if(clients[i].fd<0){slot=i;break;}if(slot>=0){memset(&clients[slot],0,sizeof(clients[slot]));clients[slot].fd=cfd;}else close(cfd);}}
        int k=1;for(int i=0;i<MAX_CLIENTS;i++)if(clients[i].fd>=0){if(pfds[k].revents&(POLLERR|POLLHUP)){remove_client(i);k++;continue;}if(pfds[k].revents&POLLIN){ssize_t n=read(clients[i].fd,buf,sizeof(buf));if(n<=0){remove_client(i);}else if(handle_raw_bytes(&clients[i],buf,(size_t)n)){if(clients[i].authenticated && dispatch_buffered(&clients[i])<0)remove_client(i);}else remove_client(i);}k++;}}
}
