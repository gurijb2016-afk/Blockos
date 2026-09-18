#pragma once
struct passwd { char *pw_name; char *pw_passwd; unsigned long pw_uid; unsigned long pw_gid; char *pw_gecos; char *pw_dir; char *pw_shell; };
struct passwd *getpwuid(unsigned long uid);
struct passwd *getpwnam(const char *name);
