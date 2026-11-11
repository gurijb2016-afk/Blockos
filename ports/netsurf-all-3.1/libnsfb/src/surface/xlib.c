/*
 * X11 surface for libnsfb using plain Xlib (libX11).
 *
 * Written for the BlockOS port of NetSurf. It replaces src/surface/x.c,
 * which needs SysV shared memory (sys/ipc.h, sys/shm.h, XShm) and the
 * xcb-util libraries (xcb-icccm, xcb-image, xcb-keysyms, xcb-atom). BlockOS
 * has neither. This surface uses only core X11 requests: XCreateImage with
 * a malloc'd buffer, XPutImage, XPending/XNextEvent, XLookupKeysym.
 *
 * The keyboard map and the keysym conversion are the same as in x.c.
 *
 * Licenced under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 */

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>

#include "libnsfb.h"
#include "libnsfb_event.h"
#include "libnsfb_plot.h"
#include "libnsfb_plot_util.h"

#include "nsfb.h"
#include "surface.h"
#include "plot.h"
#include "cursor.h"

#define X_BUTTON_LEFT 1
#define X_BUTTON_MIDDLE 2
#define X_BUTTON_RIGHT 3
#define X_BUTTON_WHEELUP 4
#define X_BUTTON_WHEELDOWN 5

/* largest PutImage sent in one request, in bytes */
#define PUSH_STRIP_BYTES (160 * 1024)

typedef struct xstate_s {
    Display *display;   /* connection to the X server */
    int screen;         /* default screen number */
    Window window;      /* the window we draw into */
    GC gc;              /* graphics context used for XPutImage */
    XImage *image;      /* the framebuffer; image->data is nsfb->ptr */
    Cursor blank_cursor;/* invisible pointer, nsfb draws its own cursor */
} xstate_t;

/* X keyboard codepage to nsfb mapping (copied from x.c) */
static enum nsfb_key_code_e XCSKeyboardMap[256] = {
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_BACKSPACE, /* XK_BackSpace */
    NSFB_KEY_TAB, /* XK_Tab */
    NSFB_KEY_UNKNOWN, /* XK_Linefeed */
    NSFB_KEY_CLEAR, /* XK_Clear */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_RETURN, /* XK_Return */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    /* 0x10 */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_PAUSE, /* XK_Pause */
    NSFB_KEY_UNKNOWN, /* XK_Scroll_Lock */
    NSFB_KEY_UNKNOWN, /* XK_Sys_Req */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_ESCAPE, /* XK_Escape */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    /* 0x20 */
    NSFB_KEY_UNKNOWN, /* XK_Multi_key */
    NSFB_KEY_UNKNOWN, /* XK_Kanji */
    NSFB_KEY_UNKNOWN, /* XK_Muhenkan */
    NSFB_KEY_UNKNOWN, /* XK_Henkan_Mode */
    NSFB_KEY_UNKNOWN, /* XK_Henkan */
    NSFB_KEY_UNKNOWN, /* XK_Romaji */
    NSFB_KEY_UNKNOWN, /* XK_Hiragana*/
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    /* 0x30 */
    NSFB_KEY_UNKNOWN, /* XK_Eisu_toggle */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* XK_Codeinput */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* XK_SingleCandidate */
    NSFB_KEY_UNKNOWN, /* XK_MultipleCandidate */
    NSFB_KEY_UNKNOWN, /* XK_PreviousCandidate */
    NSFB_KEY_UNKNOWN, /* */
    /* 0x40 */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    /* 0x50 */
    NSFB_KEY_HOME, /* XK_Home */
    NSFB_KEY_LEFT, /* XK_Left */
    NSFB_KEY_UP, /* XK_Up */
    NSFB_KEY_RIGHT, /* XK_Right */
    NSFB_KEY_DOWN, /* XK_Down */
    NSFB_KEY_PAGEUP, /* XK_Page_Up */
    NSFB_KEY_PAGEDOWN, /* XK_Page_Down */
    NSFB_KEY_END, /* XK_End */
    NSFB_KEY_UNKNOWN, /* XK_Begin */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    /* 0x60 */
    NSFB_KEY_UNKNOWN, /* XK_Select */
    NSFB_KEY_UNKNOWN, /* XK_Print*/
    NSFB_KEY_UNKNOWN, /* XK_Execute*/
    NSFB_KEY_UNKNOWN, /* XK_Insert*/
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* XK_Undo*/
    NSFB_KEY_UNKNOWN, /* XK_Redo */
    NSFB_KEY_UNKNOWN, /* XK_Menu */
    NSFB_KEY_UNKNOWN, /* XK_Find */
    NSFB_KEY_UNKNOWN, /* XK_Cancel */
    NSFB_KEY_UNKNOWN, /* XK_Help */
    NSFB_KEY_UNKNOWN, /* XK_Break*/
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    /* 0x70 */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* XK_Mode_switch */
    NSFB_KEY_UNKNOWN, /* XK_Num_Lock */
    /* 0x80 */
    NSFB_KEY_UNKNOWN, /* XK_KP_Space */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* XK_KP_Tab */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* XK_KP_Enter */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    /* 0x90 */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* XK_KP_F1*/
    NSFB_KEY_UNKNOWN, /* XK_KP_F2*/
    NSFB_KEY_UNKNOWN, /* XK_KP_F3*/
    NSFB_KEY_UNKNOWN, /* XK_KP_F4*/
    NSFB_KEY_UNKNOWN, /* XK_KP_Home*/
    NSFB_KEY_UNKNOWN, /* XK_KP_Left*/
    NSFB_KEY_UNKNOWN, /* XK_KP_Up*/
    NSFB_KEY_UNKNOWN, /* XK_KP_Right*/
    NSFB_KEY_UNKNOWN, /* XK_KP_Down*/
    NSFB_KEY_UNKNOWN, /* XK_KP_Page_Up*/
    NSFB_KEY_UNKNOWN, /* XK_KP_Page_Down*/
    NSFB_KEY_UNKNOWN, /* XK_KP_End*/
    NSFB_KEY_UNKNOWN, /* XK_KP_Begin*/
    NSFB_KEY_UNKNOWN, /* XK_KP_Insert*/
    NSFB_KEY_UNKNOWN, /* XK_KP_Delete*/
    /* 0xa0 */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* XK_KP_Multiply*/
    NSFB_KEY_UNKNOWN, /* XK_KP_Add*/
    NSFB_KEY_UNKNOWN, /* XK_KP_Separator*/
    NSFB_KEY_UNKNOWN, /* XK_KP_Subtract*/
    NSFB_KEY_UNKNOWN, /* XK_KP_Decimal*/
    NSFB_KEY_UNKNOWN, /* XK_KP_Divide*/
    /* 0xb0 */
    NSFB_KEY_UNKNOWN, /* XK_KP_0 */
    NSFB_KEY_UNKNOWN, /* XK_KP_1 */
    NSFB_KEY_UNKNOWN, /* XK_KP_2 */
    NSFB_KEY_UNKNOWN, /* XK_KP_3 */
    NSFB_KEY_UNKNOWN, /* XK_KP_4 */
    NSFB_KEY_UNKNOWN, /* XK_KP_5 */
    NSFB_KEY_UNKNOWN, /* XK_KP_6 */
    NSFB_KEY_UNKNOWN, /* XK_KP_7 */
    NSFB_KEY_UNKNOWN, /* XK_KP_8 */
    NSFB_KEY_UNKNOWN, /* XK_KP_9 */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_F1, /* XK_F1 */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* XK_KP_Equal */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_F2, /* XK_F2 */
    /* 0xc0 */
    NSFB_KEY_F3, /* XK_F3*/
    NSFB_KEY_F4, /* XK_F4*/
    NSFB_KEY_F5, /* XK_F5*/
    NSFB_KEY_F6, /* XK_F6*/
    NSFB_KEY_F7, /* XK_F7*/
    NSFB_KEY_F8, /* XK_F8*/
    NSFB_KEY_F9, /* XK_F9*/
    NSFB_KEY_F10, /* XK_F10*/
    NSFB_KEY_F11, /* XK_F11*/
    NSFB_KEY_F12, /* XK_F12*/
    NSFB_KEY_F13, /* XK_F13 */
    NSFB_KEY_F14, /* XK_F14 */
    NSFB_KEY_F15, /* XK_F15 */
    NSFB_KEY_UNKNOWN, /* XK_F16 */
    NSFB_KEY_UNKNOWN, /* XK_F17 */
    NSFB_KEY_UNKNOWN, /* XK_F18*/
    /* 0xd0 */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    /* 0xe0 */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_LSHIFT, /* XK_Shift_L*/
    NSFB_KEY_RSHIFT, /* XK_Shift_R*/
    NSFB_KEY_UNKNOWN, /* XK_Control_L*/
    NSFB_KEY_UNKNOWN, /* XK_Control_R*/
    NSFB_KEY_UNKNOWN, /* XK_Caps_Lock*/
    NSFB_KEY_UNKNOWN, /* XK_Shift_Lock*/
    NSFB_KEY_UNKNOWN, /* XK_Meta_L*/
    NSFB_KEY_UNKNOWN, /* XK_Meta_R*/
    NSFB_KEY_UNKNOWN, /* XK_Alt_L */
    NSFB_KEY_UNKNOWN, /* XK_Alt_R*/
    NSFB_KEY_UNKNOWN, /* XK_Super_L*/
    NSFB_KEY_UNKNOWN, /* XK_Super_R*/
    NSFB_KEY_UNKNOWN, /* XK_Hyper_L*/
    NSFB_KEY_UNKNOWN, /* XK_Hyper_R*/
    NSFB_KEY_UNKNOWN, /* */
    /* 0xf0 */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
};

/* Convert an X keysym into a nsfb key code (same rules as x.c) */
static enum nsfb_key_code_e
xkeysym_to_nsfbkeycode(KeySym ks)
{
    enum nsfb_key_code_e nsfb_key = NSFB_KEY_UNKNOWN;
    unsigned int codeset = (ks & 0xFF00) >> 8;
    unsigned int chrcode = ks & 0xFF;

    if (ks != NoSymbol) {
        switch (codeset) {
        case 0x00: /* Latin 1 */
        case 0x01: /* Latin 2 */
        case 0x02: /* Latin 3 */
        case 0x03: /* Latin 4 */
        case 0x04: /* Katakana */
        case 0x05: /* Arabic */
        case 0x06: /* Cyrillic */
        case 0x07: /* Greek */
        case 0x08: /* Technical */
        case 0x0A: /* Publishing */
        case 0x0C: /* Hebrew */
        case 0x0D: /* Thai */
            nsfb_key = (enum nsfb_key_code_e)chrcode;
            break;

        case 0xFF: /* Keyboard */
            nsfb_key = XCSKeyboardMap[chrcode];
            break;
        }
    }

    return nsfb_key;
}

/* Push a rectangle of the local framebuffer to the window.
 * The framebuffer is the only copy of the pixels; the server side is
 * just a presentation target, so there is no backing pixmap.
 */
static void
push_region(xstate_t *xstate, int x, int y, int width, int height)
{
    /* Send the region in horizontal strips of at most PUSH_STRIP_BYTES.
     * On BlockOS the X server copies each PutImage into the framebuffer in
     * one kernel call, and kernel code is not preempted. A full 1280x800
     * push (4 MB) therefore froze the whole system until it finished. Small
     * strips let the scheduler run between them. */
    int bytes_per_row, rows, row;

    if (width <= 0 || height <= 0)
        return;

    bytes_per_row = width * (xstate->image->bits_per_pixel / 8);
    rows = PUSH_STRIP_BYTES / (bytes_per_row ? bytes_per_row : 1);
    if (rows < 1)
        rows = 1;

    for (row = 0; row < height; row += rows) {
        int h = height - row;
        if (h > rows)
            h = rows;
        XPutImage(xstate->display,
                  xstate->window,
                  xstate->gc,
                  xstate->image,
                  x, y + row,
                  x, y + row,
                  width, h);
    }
}

/* Errors from the server are reported but do not end the process. */
static int
xlib_error_handler(Display *d, XErrorEvent *e)
{
    char buf[256];

    XGetErrorText(d, e->error_code, buf, sizeof(buf));
    fprintf(stderr, "libnsfb xlib: %s (request %d)\n", buf, e->request_code);
    return 0;
}

static bool
xcopy(nsfb_t *nsfb, nsfb_bbox_t *srcbox, nsfb_bbox_t *dstbox)
{
    xstate_t *xstate = nsfb->surface_priv;
    nsfb_bbox_t allbox;
    struct nsfb_cursor_s *cursor = nsfb->cursor;
    uint8_t *srcptr;
    uint8_t *dstptr;
    int srcx = srcbox->x0;
    int srcy = srcbox->y0;
    int dstx = dstbox->x0;
    int dsty = dstbox->y0;
    int width = dstbox->x1 - dstbox->x0;
    int height = dstbox->y1 - dstbox->y0;
    int hloop;

    nsfb_plot_add_rect(srcbox, dstbox, &allbox);

    /* clear the cursor if it is within the region to be altered */
    if ((cursor != NULL) &&
        (cursor->plotted == true) &&
        (nsfb_plot_bbox_intersect(&allbox, &cursor->loc))) {

        nsfb_cursor_clear(nsfb, cursor);
        push_region(xstate,
                    cursor->savloc.x0,
                    cursor->savloc.y0,
                    cursor->savloc.x1 - cursor->savloc.x0,
                    cursor->savloc.y1 - cursor->savloc.y0);
    }

    /* do the copy in the local framebuffer */
    srcptr = (nsfb->ptr +
              (srcy * nsfb->linelen) +
              ((srcx * nsfb->bpp) / 8));

    dstptr = (nsfb->ptr +
              (dsty * nsfb->linelen) +
              ((dstx * nsfb->bpp) / 8));

    if (width == nsfb->width) {
        memmove(dstptr, srcptr, (width * height * nsfb->bpp) / 8);
    } else {
        if (srcy > dsty) {
            for (hloop = height; hloop > 0; hloop--) {
                memmove(dstptr, srcptr, (width * nsfb->bpp) / 8);
                srcptr += nsfb->linelen;
                dstptr += nsfb->linelen;
            }
        } else {
            srcptr += height * nsfb->linelen;
            dstptr += height * nsfb->linelen;
            for (hloop = height; hloop > 0; hloop--) {
                srcptr -= nsfb->linelen;
                dstptr -= nsfb->linelen;
                memmove(dstptr, srcptr, (width * nsfb->bpp) / 8);
            }
        }
    }

    if ((cursor != NULL) &&
        (cursor->plotted == false)) {
        nsfb_cursor_plot(nsfb, cursor);
    }

    /* present the destination area */
    push_region(xstate, dstx, dsty, width, height);

    return true;
}

static int
xlib_set_geometry(nsfb_t *nsfb, int width, int height, enum nsfb_format_e format)
{
    if (nsfb->surface_priv != NULL)
        return -1; /* if were already initialised fail */

    nsfb->width = width;
    nsfb->height = height;
    nsfb->format = format;

    /* select default sw plotters for format */
    select_plotters(nsfb);

    nsfb->plotter_fns->copy = xcopy;

    return 0;
}

/* An invisible cursor: nsfb draws its own pointer into the framebuffer */
static Cursor
create_blank_cursor(Display *dpy, Window win)
{
    static const char zero[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    Pixmap pm;
    XColor black;
    Cursor cur;

    pm = XCreateBitmapFromData(dpy, win, zero, 8, 8);
    if (pm == None)
        return None;

    memset(&black, 0, sizeof(black));
    cur = XCreatePixmapCursor(dpy, pm, pm, &black, &black, 0, 0);
    XFreePixmap(dpy, pm);

    return cur;
}

static int xlib_initialise(nsfb_t *nsfb)
{
    xstate_t *xstate = nsfb->surface_priv;
    XSetWindowAttributes wa;
    XSizeHints hints;
    unsigned int depth;
    unsigned long mask;
    uint8_t *buf;
    size_t bufsize;

    if (xstate != NULL)
        return -1; /* already initialised */

    if (nsfb->bpp != 32 && nsfb->bpp != 16) {
        fprintf(stderr, "libnsfb xlib: unsupported bpp %d\n", nsfb->bpp);
        return -1;
    }

    xstate = calloc(1, sizeof(xstate_t));
    if (xstate == NULL)
        return -1;

    xstate->display = XOpenDisplay(NULL);
    if (xstate->display == NULL) {
        fprintf(stderr, "Error opening display\n");
        free(xstate);
        return -1;
    }

    XSetErrorHandler(xlib_error_handler);

    xstate->screen = DefaultScreen(xstate->display);

    /* XPutImage needs the image depth to equal the depth of the window.
     * The window is made with CopyFromParent, so it has the root depth.
     * Using a fixed depth gave BadMatch (request 72) on every push. */
    depth = DefaultDepth(xstate->display, xstate->screen);
    fprintf(stderr, "libnsfb xlib: root depth %u, framebuffer bpp %d\n",
            depth, nsfb->bpp);
    if (!((nsfb->bpp == 32 && (depth == 24 || depth == 32)) ||
          (nsfb->bpp == 16 && depth == 16))) {
        fprintf(stderr, "libnsfb xlib: bpp %d does not match X depth %u\n",
                nsfb->bpp, depth);
        XCloseDisplay(xstate->display);
        free(xstate);
        return -1;
    }

    /* the framebuffer: one contiguous buffer, rows of width*bpp/8 bytes */
    bufsize = (size_t)nsfb->width * (nsfb->height) * (nsfb->bpp / 8);
    buf = calloc(1, bufsize);
    if (buf == NULL) {
        XCloseDisplay(xstate->display);
        free(xstate);
        return -1;
    }

    xstate->image = XCreateImage(xstate->display,
                                 DefaultVisual(xstate->display, xstate->screen),
                                 depth,
                                 ZPixmap,
                                 0,
                                 (char *)buf,
                                 nsfb->width,
                                 nsfb->height,
                                 32,
                                 nsfb->width * (nsfb->bpp / 8));
    if (xstate->image == NULL) {
        fprintf(stderr, "Unable to create image\n");
        free(buf);
        XCloseDisplay(xstate->display);
        free(xstate);
        return -1;
    }

    /* publish the framebuffer to libnsfb */
    nsfb->surface_priv = xstate;
    nsfb->ptr = (uint8_t *)xstate->image->data;
    nsfb->linelen = xstate->image->bytes_per_line;

    /* window */
    memset(&wa, 0, sizeof(wa));
    wa.background_pixel = WhitePixel(xstate->display, xstate->screen);
    wa.event_mask = ExposureMask |
                    KeyPressMask |
                    KeyReleaseMask |
                    ButtonPressMask |
                    ButtonReleaseMask |
                    PointerMotionMask;
    mask = CWBackPixel | CWEventMask;

    xstate->window = XCreateWindow(xstate->display,
                                   RootWindow(xstate->display, xstate->screen),
                                   0, 0,
                                   nsfb->width, nsfb->height,
                                   0,
                                   CopyFromParent,
                                   InputOutput,
                                   CopyFromParent,
                                   mask, &wa);

    /* fixed size window, like x.c */
    memset(&hints, 0, sizeof(hints));
    hints.flags = PMinSize | PMaxSize;
    hints.min_width = hints.max_width = nsfb->width;
    hints.min_height = hints.max_height = nsfb->height;
    XSetWMNormalHints(xstate->display, xstate->window, &hints);
    XStoreName(xstate->display, xstate->window, "NetSurf");

    xstate->gc = XCreateGC(xstate->display, xstate->window, 0, NULL);

    xstate->blank_cursor = create_blank_cursor(xstate->display, xstate->window);
    if (xstate->blank_cursor != None) {
        XDefineCursor(xstate->display, xstate->window, xstate->blank_cursor);
    }

    /* show the (black) framebuffer and the window */
    push_region(xstate, 0, 0, nsfb->width, nsfb->height);
    XMapWindow(xstate->display, xstate->window);
    XFlush(xstate->display);

    return 0;
}

static int xlib_finalise(nsfb_t *nsfb)
{
    xstate_t *xstate = nsfb->surface_priv;

    if (xstate == NULL)
        return 0;

    if (xstate->blank_cursor != None)
        XFreeCursor(xstate->display, xstate->blank_cursor);

    XFreeGC(xstate->display, xstate->gc);

    /* XDestroyImage also frees the framebuffer memory */
    XDestroyImage(xstate->image);
    nsfb->ptr = NULL;

    XDestroyWindow(xstate->display, xstate->window);
    XCloseDisplay(xstate->display);

    free(xstate);
    nsfb->surface_priv = NULL;

    return 0;
}

static bool xlib_input(nsfb_t *nsfb, nsfb_event_t *event, int timeout)
{
    xstate_t *xstate = nsfb->surface_priv;
    XEvent e;

    if (xstate == NULL)
        return false;

    XFlush(xstate->display);

    if (XPending(xstate->display) == 0) {
        /* A poll (timeout 0) is made while NetSurf is busy and comes back
         * immediately. On BlockOS a process that spins without blocking
         * starves the X server and the shell, so an idle poll waits a few
         * milliseconds on the X socket instead. */
        int wait_ms = (timeout == 0) ? 5 : timeout;

        if (wait_ms > 0) {
            int confd = ConnectionNumber(xstate->display);
            fd_set rfds;
            struct timeval tv;
            int retval;

            FD_ZERO(&rfds);
            FD_SET(confd, &rfds);

            tv.tv_sec = wait_ms / 1000;
            tv.tv_usec = (wait_ms % 1000) * 1000;

            retval = select(confd + 1, &rfds, NULL, NULL, &tv);
            if (retval == 0) {
                if (timeout == 0)
                    return false; /* poll: nothing arrived */

                /* timeout, nothing happened */
                event->type = NSFB_EVENT_CONTROL;
                event->value.controlcode = NSFB_CONTROL_TIMEOUT;
                return true;
            }
            if (retval < 0)
                return false;
        } else if (timeout == 0) {
            return false;
        }
    }

    /* an event is queued or the socket is readable: blocks at most until the
     * event is complete */
    XNextEvent(xstate->display, &e);

    event->type = NSFB_EVENT_NONE;

    switch (e.type) {
    case Expose:
        push_region(xstate,
                    e.xexpose.x, e.xexpose.y,
                    e.xexpose.width, e.xexpose.height);
        XFlush(xstate->display);
        break;

    case MotionNotify:
        event->type = NSFB_EVENT_MOVE_ABSOLUTE;
        event->value.vector.x = e.xmotion.x;
        event->value.vector.y = e.xmotion.y;
        event->value.vector.z = 0;
        break;

    case ButtonPress:
    case ButtonRelease:
        event->type = (e.type == ButtonPress) ? NSFB_EVENT_KEY_DOWN
                                              : NSFB_EVENT_KEY_UP;
        event->value.keycode = NSFB_KEY_UNKNOWN;
        switch (e.xbutton.button) {
        case X_BUTTON_LEFT:
            event->value.keycode = NSFB_KEY_MOUSE_1;
            break;
        case X_BUTTON_MIDDLE:
            event->value.keycode = NSFB_KEY_MOUSE_2;
            break;
        case X_BUTTON_RIGHT:
            event->value.keycode = NSFB_KEY_MOUSE_3;
            break;
        case X_BUTTON_WHEELUP:
            event->value.keycode = NSFB_KEY_MOUSE_4;
            break;
        case X_BUTTON_WHEELDOWN:
            event->value.keycode = NSFB_KEY_MOUSE_5;
            break;
        }
        break;

    case KeyPress:
    case KeyRelease:
        event->type = (e.type == KeyPress) ? NSFB_EVENT_KEY_DOWN
                                           : NSFB_EVENT_KEY_UP;
        event->value.keycode = xkeysym_to_nsfbkeycode(XLookupKeysym(&e.xkey, 0));
        break;

    default:
        break;
    }

    return true;
}

static int xlib_claim(nsfb_t *nsfb, nsfb_bbox_t *box)
{
    struct nsfb_cursor_s *cursor = nsfb->cursor;

    if ((cursor != NULL) &&
        (cursor->plotted == true) &&
        (nsfb_plot_bbox_intersect(box, &cursor->loc))) {
        nsfb_cursor_clear(nsfb, cursor);
    }
    return 0;
}

static int
xlib_cursor(nsfb_t *nsfb, struct nsfb_cursor_s *cursor)
{
    xstate_t *xstate = nsfb->surface_priv;
    nsfb_bbox_t redraw;
    nsfb_bbox_t fbarea;

    if ((cursor != NULL) && (cursor->plotted == true)) {

        nsfb_plot_add_rect(&cursor->savloc, &cursor->loc, &redraw);

        /* screen area */
        fbarea.x0 = 0;
        fbarea.y0 = 0;
        fbarea.x1 = nsfb->width;
        fbarea.y1 = nsfb->height;

        nsfb_plot_clip(&fbarea, &redraw);

        nsfb_cursor_clear(nsfb, cursor);
        nsfb_cursor_plot(nsfb, cursor);

        push_region(xstate,
                    redraw.x0, redraw.y0,
                    redraw.x1 - redraw.x0, redraw.y1 - redraw.y0);
        XFlush(xstate->display);
    }
    return true;
}

static int xlib_update(nsfb_t *nsfb, nsfb_bbox_t *box)
{
    xstate_t *xstate = nsfb->surface_priv;
    struct nsfb_cursor_s *cursor = nsfb->cursor;

    if ((cursor != NULL) &&
        (cursor->plotted == false)) {
        nsfb_cursor_plot(nsfb, cursor);
    }

    push_region(xstate, box->x0, box->y0,
                box->x1 - box->x0, box->y1 - box->y0);
    XFlush(xstate->display);

    return 0;
}

const nsfb_surface_rtns_t xlib_rtns = {
    .initialise = xlib_initialise,
    .finalise = xlib_finalise,
    .input = xlib_input,
    .claim = xlib_claim,
    .update = xlib_update,
    .cursor = xlib_cursor,
    .geometry = xlib_set_geometry,
};

NSFB_SURFACE_DEF(xlib, NSFB_SURFACE_X, &xlib_rtns)
