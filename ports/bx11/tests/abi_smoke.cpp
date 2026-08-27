#include <cassert>
#include <cstdint>
#include <vector>
#include "X11/Xlib.h"
#include "xcb/xcb.h"

extern "C" void bx11_bind_framebuffer(uint32_t*,uint32_t,uint32_t,uint32_t);
extern "C" void bx11_xcb_bind_framebuffer(uint32_t*,uint32_t,uint32_t,uint32_t);

int main(){
    std::vector<uint32_t> pixels(320*200);
    bx11_bind_framebuffer(pixels.data(),320,200,320);
    bx11_xcb_bind_framebuffer(pixels.data(),320,200,320);
    Display*d=XOpenDisplay(":0"); assert(d);
    Window w=XCreateSimpleWindow(d,XDefaultRootWindow(d),10,10,120,80,1,0,0xFF303030u); assert(w);
    assert(XSelectInput(d,w,ExposureMask|StructureNotifyMask|KeyPressMask)==0);
    assert(XMapWindow(d,w)==0);
    assert(XMoveResizeWindow(d,w,20,20,200,100)==0);
    assert(XFlush(d)==0);
    XCloseDisplay(d);

    int screen=-1; xcb_connection_t*c=xcb_connect(":0",&screen); assert(c); assert(xcb_connection_has_error(c)==0);
    xcb_window_t xw=xcb_generate_id(c); assert(xw);
    (void)xcb_create_window(c,0,xw,1,5,5,50,50,0,1,0,0,nullptr);
    xcb_map_window(c,xw); xcb_flush(c); xcb_disconnect(c);
    return 0;
}
