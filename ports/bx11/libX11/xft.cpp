#include <cstdlib>
#include "X11/Xft.h"
struct _XftFont {};
struct _XftDraw {};
extern "C" XftFont *XftFontOpenName(Display*, int, const char*) { return nullptr; }
extern "C" void XftFontClose(Display*, XftFont*) {}
extern "C" XftDraw *XftDrawCreate(Display*, Drawable, Visual*, Colormap) { return nullptr; }
extern "C" void XftDrawDestroy(XftDraw*) {}
extern "C" Bool XftDrawStringUtf8(XftDraw*, const XftColor*, XftFont*, int, int, const unsigned char*, int) { return 0; }
