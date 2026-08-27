#pragma once
#include "../Xlib.h"
typedef unsigned long RROutput;
typedef unsigned long RRCrtc;
typedef unsigned long RRMode;
static inline int XRRQueryExtension(Display*,int*,int*){return 0;}
static inline Bool XRRQueryVersion(Display*,int *maj,int *min){if(maj)*maj=1;if(min)*min=1;return 1;}
