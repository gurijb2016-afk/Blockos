AC_DEFUN([BLOCKOS_ARCH_CHECK], [

AC_MSG_CHECKING([target architecture])

case "$host_cpu" in

x86_64|amd64)
    AC_DEFINE([BLOCKOS_X86_64],1,
    [x86_64 CPU])
    AC_MSG_RESULT([x86_64])
;;

aarch64)
    AC_DEFINE([BLOCKOS_ARM64],1,
    [ARM64 CPU])
    AC_MSG_RESULT([arm64])
;;

*)
    AC_MSG_ERROR(
    [Unsupported CPU architecture])
;;

esac

])
