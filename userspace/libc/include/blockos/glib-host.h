#ifndef BLOCKOS_GLIB_HOST_H
#define BLOCKOS_GLIB_HOST_H

/* BlockOS build/runtime feature contract for GLib-family libraries. */
#define BLOCKOS_OS 1
#define BLOCKOS_X86_64 1
#define BLOCKOS_HAVE_PROC 0
#define BLOCKOS_HAVE_SYS_SYSINFO 0
#define BLOCKOS_HAVE_LINUX_MEMFD 0
#define BLOCKOS_HAVE_LINUX_EPOLL 0
#define BLOCKOS_HAVE_LINUX_EVENTFD 0
#define BLOCKOS_HAVE_LINUX_INOTIFY 0
#define BLOCKOS_HAVE_LINUX_TIMERFD 0
#define BLOCKOS_HAVE_SYSV_SEM 0

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#endif
