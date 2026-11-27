#ifndef BLOCKOS_GLOB_H
#define BLOCKOS_GLOB_H
#ifdef __cplusplus
extern "C" {
#endif
#include <stddef.h>
typedef struct { size_t gl_pathc; char **gl_pathv; size_t gl_offs; } glob_t;
#define GLOB_ERR    0x0004
#define GLOB_MARK   0x0008
#define GLOB_NOSORT 0x0020
#define GLOB_NOCHECK 0x0010
#define GLOB_NOMATCH 3
#define GLOB_NOSPACE 1
#define GLOB_ABORTED 2
int glob(const char *pattern, int flags, int (*errfunc)(const char *, int), glob_t *pglob);
void globfree(glob_t *pglob);
#ifdef __cplusplus
}
#endif
#endif
