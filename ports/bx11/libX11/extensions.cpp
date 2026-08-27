#include <cstdlib>
#include <cstring>
#include "X11/Xlib.h"
#include "X11/Xrandr.h"
#include "X11/extensions/Xinerama.h"
#include "X11/extensions/Xrender.h"
#include "X11/extensions/shape.h"
#include "X11/extensions/Xfixes.h"

extern "C" {

Bool XRRQueryVersion(Display*, int *major, int *minor) { if (major) *major = 1; if (minor) *minor = 6; return 1; }
void *XRRGetScreenInfo(Display*, Window) { return nullptr; }
XRRScreenSize *XRRConfigSizes(void*, int *n) { if (n) *n = 0; return nullptr; }
short XRRConfigCurrentConfiguration(void*, int *rotation) { if (rotation) *rotation = 0; return 0; }
void XRRFreeScreenConfigInfo(void*) {}
int XRRSelectInput(Display*, Window, int) { return 0; }

Bool XineramaQueryExtension(Display*, int *event_base, int *error_base) { if (event_base) *event_base = 0; if (error_base) *error_base = 0; return 1; }
Bool XineramaIsActive(Display*, Window) { return 1; }
XineramaScreenInfo *XineramaQueryScreens(Display*, int *count) {
    if (count) *count = 1;
    auto *info = static_cast<XineramaScreenInfo*>(std::calloc(1, sizeof(XineramaScreenInfo)));
    if (info) { info->screen_number = 0; info->width = 1024; info->height = 768; }
    return info;
}

XRenderPictFormat *XRenderFindVisualFormat(Display*, Visual*) { return nullptr; }
int XRenderQueryVersion(Display*, int *major, int *minor) { if (major) *major = 0; if (minor) *minor = 0; return 0; }
void XRenderFreePicture(Display*, Picture) {}

Bool XShapeQueryExtension(Display*, int *event_base, int *error_base) { if (event_base) *event_base = 0; if (error_base) *error_base = 0; return 1; }
int XShapeCombineMask(Display*, Window, int, int, int, Pixmap, int) { return 0; }
int XShapeCombineRegion(Display*, Window, int, int, int, void*, int) { return 0; }

Bool XFixesQueryExtension(Display*, int *event_base, int *error_base) { if (event_base) *event_base = 0; if (error_base) *error_base = 0; return 1; }
int XFixesQueryVersion(Display*, int *major, int *minor) { if (major) *major = 6; if (minor) *minor = 0; return 1; }

}
