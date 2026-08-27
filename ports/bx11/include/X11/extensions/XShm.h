#pragma once
#include "../Xlib.h"
typedef struct { int shmseg; } XShmSegmentInfo;
static inline Bool XShmQueryExtension(Display*){return 0;}
