#pragma once
struct passwd { char *pw_name; char *pw_passwd; unsigned long pw_uid; unsigned long pw_gid; char *pw_gecos; char *pw_dir; char *pw_shell; };
struct passwd *getpwuid(unsigned long uid);
struct passwd *getpwnam(const char *name);

#include <stddef.h>
#ifndef BLOCKOS_PWD_X11_COMPAT
#define BLOCKOS_PWD_X11_COMPAT
int getpwuid_r(unsigned long uid, struct passwd *pwd, char *buf, size_t buflen, struct passwd **result);
int getpwnam_r(const char *name, struct passwd *pwd, char *buf, size_t buflen, struct passwd **result);
#endif
