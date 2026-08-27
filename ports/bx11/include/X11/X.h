#pragma once
#include <stdint.h>

#define None 0L
#define ParentRelative 1L
#define CopyFromParent 0L
#define InputOutput 1
#define InputOnly 2

#define KeyPress 2
#define KeyRelease 3
#define ButtonPress 4
#define ButtonRelease 5
#define MotionNotify 6
#define EnterNotify 7
#define LeaveNotify 8
#define FocusIn 9
#define FocusOut 10
#define Expose 12
#define DestroyNotify 17
#define UnmapNotify 18
#define MapNotify 19
#define ConfigureNotify 22
#define PropertyNotify 28
#define ClientMessage 33

#define NoEventMask 0L
#define KeyPressMask (1L << 0)
#define KeyReleaseMask (1L << 1)
#define ButtonPressMask (1L << 2)
#define ButtonReleaseMask (1L << 3)
#define PointerMotionMask (1L << 6)
#define EnterWindowMask (1L << 4)
#define LeaveWindowMask (1L << 5)
#define ExposureMask (1L << 15)
#define StructureNotifyMask (1L << 17)
#define PropertyChangeMask (1L << 22)
#define FocusChangeMask (1L << 21)
#define SubstructureNotifyMask (1L << 19)
#define SubstructureRedirectMask (1L << 20)

#define CWX (1L << 0)
#define CWY (1L << 1)
#define CWWidth (1L << 2)
#define CWHeight (1L << 3)
#define CWBorderWidth (1L << 4)
#define CWSibling (1L << 5)
#define CWStackMode (1L << 6)

#define Above 0
#define Below 1
#define TopIf 2
#define BottomIf 3
#define Opposite 4

#define PropModeReplace 0
#define PropModePrepend 1
#define PropModeAppend 2

#define XA_PRIMARY 1
#define XA_SECONDARY 2
#define XA_ARC 3
#define XA_ATOM 4
#define XA_BITMAP 5
#define XA_CARDINAL 6
#define XA_COLORMAP 7
#define XA_CURSOR 8
#define XA_CUT_BUFFER0 9
#define XA_CUT_BUFFER1 10
#define XA_CUT_BUFFER2 11
#define XA_CUT_BUFFER3 12
#define XA_CUT_BUFFER4 13
#define XA_CUT_BUFFER5 14
#define XA_CUT_BUFFER6 15
#define XA_CUT_BUFFER7 16
#define XA_DRAWABLE 17
#define XA_FONT 18
#define XA_INTEGER 19
#define XA_PIXMAP 20
#define XA_POINT 21
#define XA_RECTANGLE 22
#define XA_RESOURCE_MANAGER 23
#define XA_RGB_COLOR_MAP 24
#define XA_RGB_BEST_MAP 25
#define XA_RGB_BLUE_MAP 26
#define XA_RGB_DEFAULT_MAP 27
#define XA_RGB_GRAY_MAP 28
#define XA_RGB_GREEN_MAP 29
#define XA_RGB_RED_MAP 30
#define XA_STRING 31
#define XA_VISUALID 32
#define XA_WINDOW 33
#define XA_WM_COMMAND 34
#define XA_WM_HINTS 35
#define XA_WM_CLIENT_MACHINE 36
#define XA_WM_ICON_NAME 37
#define XA_WM_ICON_SIZE 38
#define XA_WM_NAME 39
#define XA_WM_NORMAL_HINTS 40
#define XA_WM_SIZE_HINTS 41
#define XA_WM_ZOOM_HINTS 42
#define XA_MIN_SPACE 43
#define XA_NORM_SPACE 44
#define XA_MAX_SPACE 45
#define XA_END_SPACE 46
#define XA_SUPERSCRIPT_X 47
#define XA_SUPERSCRIPT_Y 48
#define XA_SUBSCRIPT_X 49
#define XA_SUBSCRIPT_Y 50
#define XA_UNDERLINE_POSITION 51
#define XA_UNDERLINE_THICKNESS 52
#define XA_STRIKEOUT_ASCENT 53
#define XA_STRIKEOUT_DESCENT 54
#define XA_ITALIC_ANGLE 55
#define XA_X_HEIGHT 56
#define XA_QUAD_WIDTH 57
#define XA_WEIGHT 58
#define XA_POINT_SIZE 59
#define XA_RESOLUTION 60
#define XA_COPYRIGHT 61
#define XA_NOTICE 62
#define XA_FONT_NAME 63
#define XA_FULL_NAME 64
#define XA_CAP_HEIGHT 65
#define XA_WM_CLASS 67
#define XA_WM_TRANSIENT_FOR 68

typedef uint32_t XID;
typedef XID Window;
typedef XID Drawable;
typedef XID Pixmap;
typedef XID Cursor;
typedef XID Colormap;
typedef XID Font;
typedef XID GContext;
typedef uint32_t Atom;
typedef uint32_t Time;
typedef uint32_t KeySym;
typedef unsigned long EventMask;
typedef int Bool;
typedef unsigned long Mask;
typedef unsigned long VisualID;
typedef int Status;
