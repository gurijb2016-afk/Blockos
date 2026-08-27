#pragma once
#include "../Xlib.h"
#ifdef __cplusplus
extern "C" {
#endif
Bool XShapeQueryExtension(Display*, int*, int*);
int XShapeCombineMask(Display*, Window, int, int, int, Pixmap, int);
int XShapeCombineRegion(Display*, Window, int, int, int, void*, int);
#ifdef __cplusplus
}
#endif
