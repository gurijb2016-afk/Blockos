#pragma once
#include "../Xlib.h"
#ifdef __cplusplus
extern "C" {
#endif
Bool XFixesQueryExtension(Display*, int*, int*);
int XFixesQueryVersion(Display*, int*, int*);
#ifdef __cplusplus
}
#endif
