dnl =====================================================
dnl BlockOS Compiler Detection
dnl =====================================================

AC_DEFUN([BLOCKOS_CHECK_COMPILER], [

AC_REQUIRE([AC_PROG_CXX])

AC_MSG_CHECKING([C++ compiler])

AS_IF([test -n "$CXX"],[
    AC_MSG_RESULT([$CXX])
],[
    AC_MSG_ERROR([No C++ compiler found])
])


case "$CXX" in
    *g++)
        AC_DEFINE([BLOCKOS_GCC],1,
        [Using GCC compiler])
    ;;

    *clang++)
        AC_DEFINE([BLOCKOS_CLANG],1,
        [Using Clang compiler])
    ;;

    *)
        AC_MSG_WARN(
        [Unknown compiler])
    ;;
esac

])
