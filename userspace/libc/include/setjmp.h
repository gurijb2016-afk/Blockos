#ifndef BLOCKOS_SETJMP_H
#define BLOCKOS_SETJMP_H
typedef long jmp_buf[8];
typedef long sigjmp_buf[8];
int setjmp(jmp_buf env);
void longjmp(jmp_buf env, int val) __attribute__((noreturn));
#define _setjmp setjmp
#define _longjmp longjmp
#define sigsetjmp(env, save) setjmp(env)
#define siglongjmp(env, val) longjmp(env, val)
#endif
