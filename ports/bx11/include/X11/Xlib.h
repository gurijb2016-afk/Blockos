#pragma once
#include <stddef.h>
#include <stdint.h>
#include "X.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _XDisplay Display;
typedef struct _XVisual Visual;
typedef struct _XGC *GC;
typedef struct _XImage XImage;

typedef struct {
    int type;
    unsigned long serial;
    Bool send_event;
    Display *display;
    Window window;
} XAnyEvent;

typedef struct {
    int type;
    unsigned long serial;
    Bool send_event;
    Display *display;
    Window window;
    Window root;
    int x, y;
    int width, height;
    int count;
} XExposeEvent;

typedef struct {
    int type;
    unsigned long serial;
    Bool send_event;
    Display *display;
    Window event, window;
    int x, y;
    int width, height;
    int border_width;
    Window above;
    Bool override_redirect;
} XConfigureEvent;

typedef struct {
    int type;
    unsigned long serial;
    Bool send_event;
    Display *display;
    Window window;
    Atom atom;
    Time time;
    int state;
} XPropertyEvent;

typedef struct {
    int type;
    unsigned long serial;
    Bool send_event;
    Display *display;
    Window window;
    Window root;
    Window subwindow;
    Time time;
    int x, y;
    int x_root, y_root;
    unsigned int state;
    unsigned int keycode;
    Bool same_screen;
} XKeyEvent;

typedef XKeyEvent XButtonEvent;
typedef XKeyEvent XMotionEvent;

typedef struct {
    int type;
    unsigned long serial;
    Bool send_event;
    Display *display;
    Window window;
    Atom message_type;
    int format;
    union {
        char b[20];
        short s[10];
        long l[5];
    } data;
} XClientMessageEvent;

typedef union _XEvent {
    int type;
    XAnyEvent xany;
    XExposeEvent xexpose;
    XConfigureEvent xconfigure;
    XPropertyEvent xproperty;
    XKeyEvent xkey;
    XButtonEvent xbutton;
    XMotionEvent xmotion;
    XClientMessageEvent xclient;
    long pad[24];
} XEvent;

typedef struct {
    int x, y;
    unsigned int width, height;
    unsigned int border_width;
    Window root;
    int depth;
    int c_class;
    Visual *visual;
    unsigned long backing_store;
    Bool save_under;
    Colormap colormap;
    Bool map_installed;
    int map_state;
    long all_event_masks;
    long your_event_mask;
    long do_not_propagate_mask;
    Bool override_redirect;
    Visual *screen;
} XWindowAttributes;

typedef struct {
    int x, y;
    int width, height;
    int border_width;
    Window sibling;
    int stack_mode;
} XWindowChanges;

typedef struct {
    int flags;
    int x, y;
    int width, height;
    int min_width, min_height;
    int max_width, max_height;
    int width_inc, height_inc;
    int min_aspect_x, min_aspect_y;
    int max_aspect_x, max_aspect_y;
    int base_width, base_height;
    int win_gravity;
} XSizeHints;

typedef struct {
    long flags;
    Bool input;
    int initial_state;
    Pixmap icon_pixmap;
    Window icon_window;
    int icon_x, icon_y;
    Pixmap icon_mask;
    XID window_group;
} XWMHints;

typedef struct {
    char *res_name;
    char *res_class;
} XClassHint;

typedef struct {
    Atom visualid;
    void *screen;
} XVisualInfo;

Display *XOpenDisplay(const char *display_name);
int XCloseDisplay(Display *display);
const char *XDisplayName(const char *display_name);
int XDefaultScreen(Display *display);
Window XDefaultRootWindow(Display *display);
Window XRootWindow(Display *display, int screen_number);
unsigned long XBlackPixel(Display *display, int screen_number);
unsigned long XWhitePixel(Display *display, int screen_number);

Window XCreateSimpleWindow(Display *, Window, int, int, unsigned int, unsigned int,
                           unsigned int, unsigned long, unsigned long);
Window XCreateWindow(Display *, Window, int, int, unsigned int, unsigned int,
                     unsigned int, int, unsigned int, Visual *, unsigned long,
                     void *);
int XDestroyWindow(Display *, Window);
int XMapWindow(Display *, Window);
int XMapRaised(Display *, Window);
int XUnmapWindow(Display *, Window);
int XMoveWindow(Display *, Window, int, int);
int XResizeWindow(Display *, Window, unsigned int, unsigned int);
int XMoveResizeWindow(Display *, Window, int, int, unsigned int, unsigned int);
int XRaiseWindow(Display *, Window);
int XLowerWindow(Display *, Window);
int XReparentWindow(Display *, Window, Window, int, int);
int XConfigureWindow(Display *, Window, unsigned int, XWindowChanges *);
int XGetWindowAttributes(Display *, Window, XWindowAttributes *);
int XSelectInput(Display *, Window, long);
int XGetGeometry(Display *, Drawable, Window *, int *, int *, unsigned int *, unsigned int *, unsigned int *, unsigned int *);

int XNextEvent(Display *, XEvent *);
int XPending(Display *);
int XEventsQueued(Display *, int, XEvent *);
int XPutBackEvent(Display *, XEvent *);
int XSendEvent(Display *, Window, Bool, long, XEvent *);

Atom XInternAtom(Display *, const char *, Bool);
char *XGetAtomName(Display *, Atom);
int XChangeProperty(Display *, Window, Atom, Atom, int, int, const unsigned char *, int);
int XDeleteProperty(Display *, Window, Atom);
int XGetWindowProperty(Display *, Window, Atom, long, long, Bool, Atom,
                       Atom *, int *, unsigned long *, unsigned long *, unsigned char **);

int XSetWMNormalHints(Display *, Window, XSizeHints *);
void XSetWMHints(Display *, Window, XWMHints *);
int XGetWMName(Display *, Window, void *);
int XSetWMName(Display *, Window, void *);
int XSetClassHint(Display *, Window, XClassHint *);

int XFlush(Display *);
int XSync(Display *, Bool);
int XFree(void *);
int XDefineCursor(Display *, Window, Cursor);
int XUndefineCursor(Display *, Window);
int XGrabPointer(Display *, Window, Bool, unsigned int, int, int, Window, Cursor, Time);
int XUngrabPointer(Display *, Time);
int XGrabKeyboard(Display *, Window, Bool, int, int, Time);
int XUngrabKeyboard(Display *, Time);

Pixmap XCreatePixmap(Display *, Drawable, unsigned int, unsigned int, unsigned int);
int XFreePixmap(Display *, Pixmap);
GC XCreateGC(Display *, Drawable, unsigned long, void *);
int XFreeGC(Display *, GC);
int XSetForeground(Display *, GC, unsigned long);
int XSetBackground(Display *, GC, unsigned long);
int XFillRectangle(Display *, Drawable, GC, int, int, unsigned int, unsigned int);
int XDrawRectangle(Display *, Drawable, GC, int, int, unsigned int, unsigned int);
int XDrawLine(Display *, Drawable, GC, int, int, int, int);
int XCopyArea(Display *, Drawable, Drawable, GC, int, int, unsigned int, unsigned int, int, int);

#ifdef __cplusplus
}
#endif
