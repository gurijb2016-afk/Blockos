#include "pwd.h"
#include "grp.h"
#include "errno.h"
static char n_root[]="root", n_user[]="blockos", home[]="/", shell[]="/bin/sh", x[]="x";
static struct passwd pw = { n_user, x, 0, 0, n_user, home, shell };
static struct group gr = { n_root, x, 0, 0 };
struct passwd *getpwuid(unsigned long uid){ if(uid!=0){errno=ENOENT;return 0;} return &pw; }
struct passwd *getpwnam(const char* name){ if(!name || (name[0]!='r' && name[0]!='b')){errno=ENOENT;return 0;} return &pw; }
struct group *getgrgid(unsigned long gid){ if(gid!=0){errno=ENOENT;return 0;} return &gr; }
struct group *getgrnam(const char* name){ if(!name){errno=ENOENT;return 0;} return &gr; }
