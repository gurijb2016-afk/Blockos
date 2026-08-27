#include <cstdlib>
#include <cstring>
#include <vector>
#include <unordered_map>
#include <string>
#include "X11/Xlib.h"
#include "../ipc/transport.hpp"
#include "../server/server.hpp"
#include "../server/protocol.hpp"

struct _XDisplay {
    blockos::bx11::ipc::RingChannel channel;
    uint32_t client_id{0};
    blockos::bx11::server::Server *server{nullptr};
    std::unordered_map<Window,long> masks;
    std::vector<XEvent> events;
};
struct _XGC { unsigned long foreground{0xFFFFFFFFu}; unsigned long background{0}; };
struct _XVisual {};
struct _XImage {};

static blockos::bx11::render::Framebuffer test_fb{};
static blockos::bx11::render::Surface test_surface{};
static blockos::bx11::server::Server test_server{};
static uint32_t next_window_for_client = 100;

/* Minimal X Resource Manager compatibility hook required by WindowMaker. */
extern "C" void XrmInitialize() {}

extern "C" Display *XOpenDisplay(const char *)
{
    if(!test_fb.pixels) return nullptr;
    static bool initialized=false;
    if(!initialized){ test_surface.bind(test_fb); test_server.init(&test_surface); initialized=true; }
    auto *d=new Display{}; d->server=&test_server; d->client_id=test_server.connect(&d->channel); return d;
}

extern "C" int XCloseDisplay(Display *d){ if(!d)return 0; if(d->server)d->server->disconnect(d->client_id); delete d; return 0; }
extern "C" const char *XDisplayName(const char *n){ return n?n:":0"; }
extern "C" int XDefaultScreen(Display*){return 0;}
extern "C" Window XDefaultRootWindow(Display*){return 1;}
extern "C" Window XRootWindow(Display*,int){return 1;}
extern "C" unsigned long XBlackPixel(Display*,int){return 0;}
extern "C" unsigned long XWhitePixel(Display*,int){return 0xFFFFFFFFu;}

extern "C" int XFlush(Display *d){ if(!d||!d->server)return 0; d->server->render(); return 0; }
extern "C" int XSync(Display *d,Bool){return XFlush(d);}
extern "C" int XFree(void *p){std::free(p);return 0;}

extern "C" Window XCreateSimpleWindow(Display *d,Window parent,int x,int y,unsigned w,unsigned h,unsigned border,unsigned long borderc,unsigned long bg){
    (void)borderc;
    if(!d||!d->server) return 0;
    return d->server->create_window(d->client_id,parent,x,y,w,h,border,bg);
}
extern "C" Window XCreateWindow(Display*d,Window parent,int x,int y,unsigned w,unsigned h,unsigned border,int depth,unsigned c_class,Visual* visual,unsigned long valuemask,void* attributes){(void)depth;(void)c_class;(void)visual;(void)valuemask;(void)attributes;return XCreateSimpleWindow(d,parent,x,y,w,h,border,0,0xFF202020u);}
extern "C" int XDestroyWindow(Display*d,Window w){return d&&d->server&&d->server->destroy_window(w)?0:-1;}
extern "C" int XMapWindow(Display*d,Window w){return d&&d->server&&d->server->map_window(w)?0:-1;}
extern "C" int XMapRaised(Display*d,Window w){return XMapWindow(d,w);}
extern "C" int XUnmapWindow(Display*d,Window w){return d&&d->server&&d->server->unmap_window(w)?0:-1;}
extern "C" int XMoveWindow(Display*d,Window w,int x,int y){auto *win=d&&d->server?d->server->find_window(w):nullptr;return win&&d->server->move_resize(w,x,y,win->width,win->height)?0:-1;}
extern "C" int XResizeWindow(Display*d,Window w,unsigned a,unsigned b){auto *win=d&&d->server?d->server->find_window(w):nullptr;return win&&d->server->move_resize(w,win->x,win->y,a,b)?0:-1;}
extern "C" int XMoveResizeWindow(Display*d,Window w,int x,int y,unsigned a,unsigned b){return d&&d->server&&d->server->move_resize(w,x,y,a,b)?0:-1;}
extern "C" int XRaiseWindow(Display*d,Window w){(void)d;(void)w;return 0;}
extern "C" int XLowerWindow(Display*d,Window w){(void)d;(void)w;return 0;}
extern "C" int XReparentWindow(Display*d,Window w,Window p,int x,int y){auto *win=d&&d->server?d->server->find_window(w):nullptr; if(!win||!d->server->find_window(p))return -1;win->parent=p;return d->server->move_resize(w,x,y,win->width,win->height)?0:-1;}
extern "C" int XConfigureWindow(Display*d,Window w,unsigned m,XWindowChanges*c){if(!d||!c)return -1;auto *win=d->server->find_window(w);if(!win)return -1;int x=(m&CWX)?c->x:win->x;int y=(m&CWY)?c->y:win->y;unsigned ww=(m&CWWidth)?c->width:win->width;unsigned hh=(m&CWHeight)?c->height:win->height;return d->server->move_resize(w,x,y,ww,hh)?0:-1;}
extern "C" int XGetWindowAttributes(Display*d,Window w,XWindowAttributes*a){if(!d||!a)return 0;auto *win=d->server->find_window(w);if(!win)return 0;std::memset(a,0,sizeof(*a));a->x=win->x;a->y=win->y;a->width=win->width;a->height=win->height;a->border_width=win->border_width;a->root=1;a->map_state=win->mapped?2:0;a->your_event_mask=win->event_mask;return 1;}
extern "C" int XSelectInput(Display*d,Window w,long mask){if(!d||!d->server)return -1;d->masks[w]=mask;return d->server->select_input(w,static_cast<uint64_t>(mask))?0:-1;}
extern "C" int XGetGeometry(Display*d,Drawable dr,Window*r,int*x,int*y,unsigned*w,unsigned*h,unsigned*border,unsigned*depth){(void)dr;if(!d||!d->server)return 0;auto *win=d->server->find_window(dr);if(!win)return 0;if(r)*r=1;if(x)*x=win->x;if(y)*y=win->y;if(w)*w=win->width;if(h)*h=win->height;if(border)*border=win->border_width;if(depth)*depth=32;return 1;}
extern "C" int XNextEvent(Display*d,XEvent*e){if(!d||!e)return 0;if(d->events.empty())return 0;*e=d->events.front();d->events.erase(d->events.begin());return 0;}
extern "C" int XPending(Display*d){return d?static_cast<int>(d->events.size()):0;}
extern "C" int XEventsQueued(Display*d,int,XEvent*){return XPending(d);}
extern "C" int XPutBackEvent(Display*d,XEvent*e){if(!d||!e)return 0;d->events.insert(d->events.begin(),*e);return 0;}
extern "C" int XSendEvent(Display*d,Window,Bool,long,XEvent*){(void)d;return 1;}
extern "C" Atom XInternAtom(Display*d,const char*n,Bool only){return d&&d->server?d->server->intern_atom(n?n:"",only)!=0?d->server->intern_atom(n?n:"",only):0:0;}
extern "C" char *XGetAtomName(Display*d,Atom a){(void)d;(void)a;return nullptr;}
extern "C" int XChangeProperty(Display*,Window,Atom,Atom,int,int,const unsigned char*,int){return 0;}
extern "C" int XDeleteProperty(Display*,Window,Atom){return 0;}
extern "C" int XGetWindowProperty(Display*,Window,Atom,long,long,Bool,Atom,Atom*,int*,unsigned long*,unsigned long*,unsigned char**){return 1;}
extern "C" int XSetWMNormalHints(Display*,Window,XSizeHints*){return 1;}
extern "C" void XSetWMHints(Display*,Window,XWMHints*){}
extern "C" int XGetWMName(Display*,Window,void*){return 0;}
extern "C" int XSetWMName(Display*,Window,void*){return 0;}
extern "C" int XSetClassHint(Display*,Window,XClassHint*){return 1;}
extern "C" int XDefineCursor(Display*,Window,Cursor){return 0;}
extern "C" int XUndefineCursor(Display*,Window){return 0;}
extern "C" int XGrabPointer(Display*,Window,Bool,unsigned int,int,int,Window,Cursor,Time){return 0;}
extern "C" int XUngrabPointer(Display*,Time){return 0;}
extern "C" int XGrabKeyboard(Display*,Window,Bool,int,int,Time){return 0;}
extern "C" int XUngrabKeyboard(Display*,Time){return 0;}
extern "C" Pixmap XCreatePixmap(Display*,Drawable,unsigned int,unsigned int,unsigned int){return next_window_for_client++;}
extern "C" int XFreePixmap(Display*,Pixmap){return 0;}
extern "C" GC XCreateGC(Display*,Drawable,unsigned long,void*){return new _XGC{};}
extern "C" int XFreeGC(Display*,GC g){delete g;return 0;}
extern "C" int XSetForeground(Display*,GC g,unsigned long c){if(g)g->foreground=c;return 0;}
extern "C" int XSetBackground(Display*,GC g,unsigned long c){if(g)g->background=c;return 0;}
extern "C" int XFillRectangle(Display*d,Drawable,GC g,int x,int y,unsigned w,unsigned h){(void)g;(void)d;(void)x;(void)y;(void)w;(void)h;return 0;}
extern "C" int XDrawRectangle(Display*,Drawable,GC,int,int,unsigned,unsigned){return 0;}
extern "C" int XDrawLine(Display*,Drawable,GC,int,int,int,int){return 0;}
extern "C" int XCopyArea(Display*,Drawable,Drawable,GC,int,int,unsigned,unsigned,int,int){return 0;}

extern "C" void bx11_bind_framebuffer(uint32_t *pixels,uint32_t width,uint32_t height,uint32_t stride){test_fb={pixels,width,height,stride};}
