#pragma once
#include "Xlib.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { unsigned long flags; int x,y,width,height; int mwidth,mheight; } XRRScreenSize;
typedef struct { int type; unsigned long serial; Bool send_event; Display *display; Window window; int rotation; Time timestamp, config_timestamp; unsigned long sizeID; unsigned short subpixel_order; } XRRScreenChangeNotifyEvent;
int XRRQueryVersion(Display*, int*, int*);
void *XRRGetScreenInfo(Display*, Window);
XRRScreenSize *XRRConfigSizes(void*, int*);
short XRRConfigCurrentConfiguration(void*, int*);
void XRRFreeScreenConfigInfo(void*);
int XRRSelectInput(Display*, Window, int);
#ifdef __cplusplus
}
#endif
