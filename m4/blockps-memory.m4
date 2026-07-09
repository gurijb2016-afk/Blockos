dnl =====================================================
dnl Memory System Checks
dnl =====================================================

AC_DEFUN([BLOCKOS_MEMORY_SYSTEM], [

AC_CHECK_SIZEOF(
[void*])

AS_IF(
[test "$ac_cv_sizeof_void_p" = 8],
[
AC_DEFINE(
[BLOCKOS_64BIT_MEMORY],
1,
[64 bit memory model])
],
[
AC_MSG_ERROR(
[BlockOS requires 64 bit system])
])

])
