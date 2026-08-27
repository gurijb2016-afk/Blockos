#pragma once
#include "../Xlib.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef unsigned long Picture;
typedef struct { short red, redMask, green, greenMask, blue, blueMask, alpha, alphaMask; } XRenderColor;
typedef struct { unsigned short red, green, blue, alpha; } XRenderDirectFormat;
typedef struct { unsigned long id; unsigned int type, depth; XRenderDirectFormat direct; } XRenderPictFormat;
XRenderPictFormat *XRenderFindVisualFormat(Display*, Visual*);
int XRenderQueryVersion(Display*, int*, int*);
void XRenderFreePicture(Display*, Picture);
#ifdef __cplusplus
}
#endif
