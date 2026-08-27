#pragma once
#include "../Xlib.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { int screen_number; short x_org,y_org; unsigned short width,height; } XineramaScreenInfo;
Bool XineramaQueryExtension(Display*, int*, int*);
Bool XineramaIsActive(Display*, Window);
XineramaScreenInfo *XineramaQueryScreens(Display*, int*);
#ifdef __cplusplus
}
#endif
