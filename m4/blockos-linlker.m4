AC_DEFUN([BLOCKOS_LINKER_CHECK], [

AC_CHECK_TOOL(
[LD],
[ld])

AS_IF(
[test -z "$LD"],
[
AC_MSG_ERROR(
[Linker not found])
])

AC_DEFINE(
[BLOCKOS_CUSTOM_LINKER],
1,
[Using kernel linker])

])
