#include <string.h>
#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include "fcntl.h"
#include "errno.h"
#include "string.h"
#include <stdint.h>


static void putc_buf(char* buf, size_t size, size_t* pos, char c){if(*pos+1<size)buf[*pos]=c;(*pos)++;}
static void puts_buf(char* buf,size_t size,size_t*pos,const char*s){if(!s)s="(null)";while(*s)putc_buf(buf,size,pos,*s++);}
static void put_uint(char*buf,size_t size,size_t*pos,unsigned long v,int base,int upper){char t[32];int n=0;const char*d=upper?"0123456789ABCDEF":"0123456789abcdef";if(!v){putc_buf(buf,size,pos,'0');return;}while(v&&n<(int)sizeof(t)){t[n++]=d[v%(unsigned)base];v/=(unsigned)base;}while(n)putc_buf(buf,size,pos,t[--n]);}
static void put_int(char*buf,size_t size,size_t*pos,long v){if(v<0){putc_buf(buf,size,pos,'-');put_uint(buf,size,pos,(unsigned long)(-(unsigned long)v),10,0);}else put_uint(buf,size,pos,(unsigned long)v,10,0);}
/* Full printf-style formatter: flags (- 0 + space #), width and precision
 * (including '*'), length modifiers (hh h l ll z j t L), and conversions
 * d i u o x X p c s f F e g n %. Every conversion consumes exactly the
 * argument it names, so argument lists stay aligned even for specifiers the
 * formatter only approximates (e/g are printed in fixed notation). */
typedef struct { char *buf; size_t size; size_t pos; } fmt_out;

static void fo_put(fmt_out *o, char c) {
    if (o->pos + 1 < o->size) o->buf[o->pos] = c;
    o->pos++;
}

static void fo_rep(fmt_out *o, char c, int n) {
    while (n-- > 0) fo_put(o, c);
}

static void fo_str(fmt_out *o, const char *s, size_t n, int width, int left) {
    int pad = (width > (int)n) ? width - (int)n : 0;
    if (!left) fo_rep(o, ' ', pad);
    for (size_t i = 0; i < n; i++) fo_put(o, s[i]);
    if (left) fo_rep(o, ' ', pad);
}

static int fo_ulltoa(unsigned long long v, int base, int upper, char *out) {
    char tmp[32];
    int n = 0;
    const char *d = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    do { tmp[n++] = d[v % (unsigned)base]; v /= (unsigned)base; } while (v);
    for (int i = 0; i < n; i++) out[i] = tmp[n - 1 - i];
    return n;
}

/* Integer field: prefix (sign or 0x), digits, optional precision, width. */
static void fo_number(fmt_out *o, const char *prefix, const char *dig, int ndig,
                      int width, int left, int zero, int prec_given, int prec) {
    int nprefix = (int)strlen(prefix);
    int zeros = (prec_given && prec > ndig) ? prec - ndig : 0;
    int total = nprefix + zeros + ndig;
    int pad = (width > total) ? width - total : 0;
    int zpad = (zero && !left && !prec_given);
    if (!left && !zpad) { fo_rep(o, ' ', pad); pad = 0; }
    for (int i = 0; i < nprefix; i++) fo_put(o, prefix[i]);
    if (zpad) { fo_rep(o, '0', pad); }
    fo_rep(o, '0', zeros);
    for (int i = 0; i < ndig; i++) fo_put(o, dig[i]);
    if (left) fo_rep(o, ' ', pad);
}

static void fo_fixed(fmt_out *o, double d, int prec_given, int prec, int width, int left, int zero, int plus, int space) {
    char prefix[2] = { 0, 0 };
    if (d < 0 || (d == 0 && 1 / d < 0)) { prefix[0] = '-'; d = -d; }
    else if (plus) prefix[0] = '+';
    else if (space) prefix[0] = ' ';
    int p = prec_given ? prec : 6;
    if (p > 9) p = 9;
    double scale = 1;
    for (int i = 0; i < p; i++) scale *= 10;
    unsigned long long ip = (unsigned long long)d;
    unsigned long long frac = (unsigned long long)((d - (double)ip) * scale + 0.5);
    if (frac >= (unsigned long long)scale && p > 0) { ip += 1; frac -= (unsigned long long)scale; }
    char dig[64]; int n = fo_ulltoa(ip, 10, 0, dig);
    if (p > 0) {
        dig[n++] = '.';
        char fr[16]; int fn = fo_ulltoa(frac, 10, 0, fr);
        for (int i = fn; i < p; i++) dig[n++] = '0';   /* leading zeros of fraction */
        for (int i = 0; i < fn; i++) dig[n++] = fr[i];
    }
    fo_number(o, prefix, dig, n, width, left, zero, 0, 0);
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap) {
    fmt_out o = { buf, size, 0 };
    for (const char *f = fmt; f && *f; f++) {
        if (*f != '%') { fo_put(&o, *f); continue; }
        f++;
        if (!*f) break;

        int left = 0, zero = 0, plus = 0, space = 0, alt = 0;
        for (;; f++) {
            if (*f == '-') left = 1;
            else if (*f == '0') zero = 1;
            else if (*f == '+') plus = 1;
            else if (*f == ' ') space = 1;
            else if (*f == '#') alt = 1;
            else break;
        }
        int width = 0;
        if (*f == '*') {
            width = va_arg(ap, int);
            if (width < 0) { left = 1; width = -width; }
            f++;
        } else {
            while (*f >= '0' && *f <= '9') { width = width * 10 + (*f - '0'); f++; }
        }
        int prec = 0, prec_given = 0;
        if (*f == '.') {
            prec_given = 1;
            f++;
            if (*f == '*') {
                prec = va_arg(ap, int);
                if (prec < 0) { prec_given = 0; prec = 0; }
                f++;
            } else {
                while (*f >= '0' && *f <= '9') { prec = prec * 10 + (*f - '0'); f++; }
            }
        }
        int lng = 0, shrt = 0;
        for (;; f++) {
            if (*f == 'h') shrt++;
            else if (*f == 'l') lng++;
            else if (*f == 'z' || *f == 'j' || *f == 't' || *f == 'q' || *f == 'L') lng = 2;
            else break;
        }
        char c = *f;
        if (!c) break;
        char dig[64];
        switch (c) {
        case 'd':
        case 'i': {
            long long v = (lng ? va_arg(ap, long long) : (long long)va_arg(ap, int));
            if (shrt && !lng) v = (short)v;
            unsigned long long mag = (v < 0) ? (unsigned long long)(-(v + 1)) + 1ULL : (unsigned long long)v;
            const char *prefix = (v < 0) ? "-" : plus ? "+" : space ? " " : "";
            int n = (prec_given && prec == 0 && mag == 0) ? 0 : fo_ulltoa(mag, 10, 0, dig);
            fo_number(&o, prefix, dig, n, width, left, zero, prec_given, prec);
            break;
        }
        case 'u':
        case 'o':
        case 'x':
        case 'X': {
            unsigned long long v = lng ? va_arg(ap, unsigned long long) : (unsigned long long)va_arg(ap, unsigned int);
            if (shrt && !lng) v = (unsigned short)v;
            int base = (c == 'u') ? 10 : (c == 'o') ? 8 : 16;
            const char *prefix = "";
            if (alt && v != 0) prefix = (c == 'x') ? "0x" : (c == 'X') ? "0X" : "";
            int n = (prec_given && prec == 0 && v == 0) ? 0 : fo_ulltoa(v, base, c == 'X', dig);
            if (alt && c == 'o' && (n == 0 || dig[0] != '0')) { /* leading zero */
                for (int i = n; i > 0; i--) dig[i] = dig[i - 1];
                dig[0] = '0'; n++;
            }
            fo_number(&o, prefix, dig, n, width, left, zero, prec_given, prec);
            break;
        }
        case 'p': {
            unsigned long long v = (unsigned long long)(uintptr_t)va_arg(ap, void *);
            int n = fo_ulltoa(v, 16, 0, dig);
            fo_number(&o, "0x", dig, n, width, left, zero, 0, 0);
            break;
        }
        case 'c': {
            char ch = (char)va_arg(ap, int);
            fo_str(&o, &ch, 1, width, left);
            break;
        }
        case 's': {
            const char *s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            size_t n = 0;
            if (prec_given) { while (n < (size_t)prec && s[n]) n++; }
            else n = strlen(s);
            fo_str(&o, s, n, width, left);
            break;
        }
        case 'f':
        case 'F':
        case 'e':
        case 'E':
        case 'g':
        case 'G': {
            double d = va_arg(ap, double);
            fo_fixed(&o, d, prec_given, prec, width, left, zero, plus, space);
            break;
        }
        case 'n': {
            int *np = va_arg(ap, int *);
            if (np) *np = (int)o.pos;
            break;
        }
        case '%':
            fo_put(&o, '%');
            break;
        default:
            /* unknown conversion: print it verbatim, consume nothing */
            fo_put(&o, '%');
            fo_put(&o, c);
            break;
        }
    }
    if (size) buf[o.pos < size ? o.pos : size - 1] = 0;
    return (int)o.pos;
}
int snprintf(char*buf,size_t size,const char*fmt,...){va_list ap;va_start(ap,fmt);int r=vsnprintf(buf,size,fmt,ap);va_end(ap);return r;}
int printf(const char*fmt,...){char b[512];va_list ap;va_start(ap,fmt);int n=vsnprintf(b,sizeof(b),fmt,ap);va_end(ap);size_t w=n>=(int)sizeof(b)?sizeof(b)-1:(size_t)n;if(w)write(1,b,w);return n;}

static unsigned char in_buf[BUFSIZ], out_buf[BUFSIZ], err_buf[BUFSIZ];
static FILE in_file={0,1,in_buf,sizeof(in_buf),0,0,0};
static FILE out_file={1,2,out_buf,sizeof(out_buf),0,0,0};
static FILE err_file={2,2,err_buf,sizeof(err_buf),0,0,0};
FILE *stdin=&in_file; FILE *stdout=&out_file; FILE *stderr=&err_file;
static int push_raw(FILE*f){if(!f||!f->buf||!f->len)return 0;ssize_t n=write(f->fd,f->buf,f->len);if(n<0)return EOF;f->len=0;f->pos=0;return 0;}
int fflush(FILE*f){if(!f)return -1;return push_raw(f);}
static FILE* alloc_stream(int fd,int flags){FILE*f=(FILE*)malloc(sizeof(FILE));if(!f)return 0;memset(f,0,sizeof(*f));f->fd=fd;f->flags=(unsigned)flags;f->buf=(unsigned char*)malloc(BUFSIZ);f->buf_size=f->buf?BUFSIZ:0;if(!f->buf){free(f);return 0;}return f;}
static int mode_flags(const char*m,int*flags){if(!m||!m[0])return -1;int f=0;switch(m[0]){case 'r':f=O_RDONLY;break;case 'w':f=O_WRONLY|O_CREAT|O_TRUNC;break;case 'a':f=O_WRONLY|O_CREAT|O_APPEND;break;default:return -1;}if(m[1]=='+')f=(f&~3)|O_RDWR;return f;}
FILE* fopen(const char*path,const char*mode){int f=0;if(mode_flags(mode,&f)<0){errno=22;return 0;}int fd=open(path,f,0666);if(fd<0)return 0;FILE*s=alloc_stream(fd,2);if(!s){close(fd);return 0;}return s;}
FILE* fdopen(int fd,const char*mode){int f=0;if(mode_flags(mode,&f)<0){errno=22;return 0;}return alloc_stream(fd,2);}
int fclose(FILE*f){if(!f||f==stdin||f==stdout||f==stderr){errno=22;return -1;}int rc=fflush(f);int c=close(f->fd);free(f->buf);free(f);return rc<0?rc:c;}
size_t fread(void*ptr,size_t sz,size_t nm,FILE*f){if(!f||!ptr||!sz)return 0;size_t want=sz*nm,got=0;unsigned char*dst=(unsigned char*)ptr;if(f->ungot_has&&want){*dst=f->ungot_ch;f->ungot_has=0;dst++;want--;got=1;if(!want)return 1;}if(want){ssize_t n=read(f->fd,dst,want);if(n<0){f->flags|=4;return got/sz;}if(n==0)f->flags|=1;got+=(size_t)n;}return got/sz;}
size_t fwrite(const void*ptr,size_t sz,size_t nm,FILE*f){if(!f||!ptr||!sz)return 0;size_t n=sz*nm;const unsigned char*p=ptr;size_t done=0;if(!f->buf)return 0;while(n){size_t room=f->buf_size-f->len;size_t k=n<room?n:room;memcpy(f->buf+f->len,p,k);f->len+=k;p+=k;n-=k;done+=k;if(f->len==f->buf_size&&push_raw(f)<0)break;}if(f==stderr)push_raw(f); /* stderr is unbuffered */return done/sz;}
int fgetc(FILE*f){unsigned char c;if(!f)return EOF;if(f->ungot_has){f->ungot_has=0;return f->ungot_ch;}return fread(&c,1,1,f)==1?c:EOF;}
int ungetc(int c,FILE*f){if(c==EOF||!f||f->ungot_has)return EOF;f->ungot_ch=(unsigned char)c;f->ungot_has=1;f->flags&=~1u;return (unsigned char)c;}int getc(FILE*f){return fgetc(f);}int fputc(int c,FILE*f){unsigned char x=(unsigned char)c;return fwrite(&x,1,1,f)==1?x:EOF;}int putc(int c,FILE*f){return fputc(c,f);}
char*fgets(char*s,int n,FILE*f){if(!s||n<=0||!f)return 0;int i=0;while(i<n-1){int c=fgetc(f);if(c==EOF)break;s[i++]=(char)c;if(c=='\n')break;}if(i==0)return 0;s[i]=0;return s;}
int fputs(const char*s,FILE*f){if(!s)return EOF;size_t n=strlen(s);return fwrite(s,1,n,f)==n?0:EOF;}int puts(const char*s){if(fputs(s,stdout)==EOF)return EOF;return fputc('\n',stdout);}
int fileno(FILE*f){if(!f){errno=22;return -1;}return f->fd;}
int fseek(FILE*f,off_t off,int whence){if(!f){errno=22;return -1;}f->ungot_has=0;if(fflush(f)<0)return -1;off_t r=lseek(f->fd,off,whence);if(r<0)return -1;f->offset=r;f->flags&=~1u;return 0;}
off_t ftell(FILE*f){if(!f){errno=22;return -1;}return lseek(f->fd,0,SEEK_CUR);}
void rewind(FILE*f){if(f)fseek(f,0,SEEK_SET);}
int feof(FILE*f){return f?(f->flags&1)!=0:0;}int ferror(FILE*f){return f?(f->flags&4)!=0:1;}void clearerr(FILE*f){if(f)f->flags&=~5u;}
int setvbuf(FILE*f,char*b,int mode,size_t n){(void)mode;if(!f)return -1;if(fflush(f)<0)return -1;if(f->buf&&f!=stdin&&f!=stdout&&f!=stderr)free(f->buf);if(mode==_IONBF){f->buf=0;f->buf_size=0;return 0;}f->buf=b;if(!f->buf)f->buf=(unsigned char*)malloc(n?n:BUFSIZ);f->buf_size=f->buf?(n?n:BUFSIZ):0;return f->buf?0:-1;}
int vfprintf(FILE*f,const char*fmt,va_list ap){char local[4096];va_list aq;va_copy(aq,ap);int n=vsnprintf(local,sizeof(local),fmt,aq);va_end(aq);if(n<0)return -1;if((size_t)n<sizeof(local))return fwrite(local,1,(size_t)n,f)==(size_t)n?n:-1;char*buf=(char*)malloc((size_t)n+1);if(!buf)return -1;va_copy(aq,ap);vsnprintf(buf,(size_t)n+1,fmt,aq);va_end(aq);int r=fwrite(buf,1,(size_t)n,f)==(size_t)n?n:-1;free(buf);return r;}
int fprintf(FILE*f,const char*fmt,...){va_list ap;va_start(ap,fmt);int r=vfprintf(f,fmt,ap);va_end(ap);return r;}
void perror(const char*s){fprintf(stderr,"%s: errno=%d\\n",s?s:"",errno);}
