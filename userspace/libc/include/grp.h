#pragma once
struct group { char *gr_name; char *gr_passwd; unsigned long gr_gid; char **gr_mem; };
struct group *getgrgid(unsigned long gid);
struct group *getgrnam(const char *name);
