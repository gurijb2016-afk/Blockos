dnl =====================================================
dnl BlockOS Environment Setup
dnl =====================================================

AC_DEFUN([BLOCKOS_ENVIRONMENT], [

AC_MSG_NOTICE(
[Configuring BlockOS build environment])


AC_DEFINE_UNQUOTED(
[BLOCKOS_VERSION],
["$PACKAGE_VERSION"],
[BlockOS version])


AC_DEFINE_UNQUOTED(
[BLOCKOS_BUILD_DATE],
["`date`"],
[Build date])


AC_DEFINE_UNQUOTED(
[BLOCKOS_TARGET],
["$host"],
[Target system])


])
