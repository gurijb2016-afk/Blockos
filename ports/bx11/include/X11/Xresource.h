#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Minimal Xrm ABI surface required by WindowMaker configuration/runtime. */
typedef void *XrmDatabase;
typedef void *XrmOptionDescRec;
void XrmInitialize(void);

#ifdef __cplusplus
}
#endif
