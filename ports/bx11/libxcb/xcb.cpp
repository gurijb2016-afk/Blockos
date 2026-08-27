#include "xcb/xcb.h"
#include "../ipc/transport.hpp"
#include "../server/server.hpp"
#include <cstdlib>
#include <vector>

struct xcb_connection_t { blockos::bx11::ipc::RingChannel channel; blockos::bx11::server::Server *server; uint32_t client_id; uint32_t next_id; int error; };
static blockos::bx11::render::Framebuffer fb{}; static blockos::bx11::render::Surface surface{}; static blockos::bx11::server::Server server{}; static bool initd=false;
static void ensure(){if(!initd){surface.bind(fb);server.init(&surface);initd=true;}}
extern "C" xcb_connection_t *xcb_connect(const char*,int *screen){ensure();auto *c=new xcb_connection_t{};c->server=&server;c->client_id=server.connect(&c->channel);c->next_id=1000;c->error=0;if(screen)*screen=0;return c;}
extern "C" int xcb_connection_has_error(xcb_connection_t*c){return c?c->error:1;}
extern "C" void xcb_disconnect(xcb_connection_t*c){if(!c)return;server.disconnect(c->client_id);delete c;}
extern "C" xcb_window_t xcb_generate_id(xcb_connection_t*c){return c?c->next_id++:0;}
extern "C" xcb_void_cookie_t xcb_create_window(xcb_connection_t*c,uint8_t,xcb_window_t wid,xcb_window_t parent,int16_t x,int16_t y,uint16_t w,uint16_t h,uint16_t border,uint16_t,uint32_t,uint32_t,const uint32_t*){if(!c)return {0,0,0};auto id=server.create_window(c->client_id,parent,x,y,w,h,border,0xFF202020u);if(id!=wid)c->error=1;return {0,1,0};}
extern "C" xcb_void_cookie_t xcb_map_window(xcb_connection_t*c,xcb_window_t w){if(!c||!server.map_window(w))return {0,0,0};return {0,1,0};}
extern "C" xcb_void_cookie_t xcb_unmap_window(xcb_connection_t*c,xcb_window_t w){if(!c||!server.unmap_window(w))return {0,0,0};return {0,1,0};}
extern "C" xcb_void_cookie_t xcb_destroy_window(xcb_connection_t*c,xcb_window_t w){if(!c||!server.destroy_window(w))return {0,0,0};return {0,1,0};}
extern "C" xcb_generic_event_t *xcb_wait_for_event(xcb_connection_t*){return nullptr;}
extern "C" int xcb_flush(xcb_connection_t*){server.render();return 0;}
extern "C" void bx11_xcb_bind_framebuffer(uint32_t*p,uint32_t w,uint32_t h,uint32_t s){fb={p,w,h,s};}
