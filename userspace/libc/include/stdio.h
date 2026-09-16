#pragma once
#include <stddef.h>
#include <stdarg.h>

/*
 * No fopen/fread/fwrite/FILE* in this MVP - that needs a real buffered
 * stream layer, which is a separate, sizeable chunk of work on its own.
 * What's here covers formatted output to a string or straight to a file
 * descriptor, which is what most early ports actually need first (Xlib
 * error paths, simple diagnostics, etc).
 */
int vsnprintf(char* buf, size_t size, const char* fmt, va_list ap);
int snprintf(char* buf, size_t size, const char* fmt, ...);

/* Writes formatted output directly to fd 1 (stdout) via write(). */
int printf(const char* fmt, ...);
