#pragma once
#include "Xlib.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct _XftFont XftFont;
typedef struct _XftDraw XftDraw;
typedef struct _FcPattern FcPattern;
typedef struct { unsigned int pixel; unsigned short red, green, blue, alpha; } XftColor;
XftFont *XftFontOpenName(Display*, int, const char*);
void XftFontClose(Display*, XftFont*);
XftDraw *XftDrawCreate(Display*, Drawable, Visual*, Colormap);
void XftDrawDestroy(XftDraw*);
Bool XftDrawStringUtf8(XftDraw*, const XftColor*, XftFont*, int, int, const unsigned char*, int);
#ifdef __cplusplus
}
#endif
