dnl =====================================================
dnl Kernel Build Validation
dnl =====================================================

AC_DEFUN([BLOCKOS_KERNEL_CHECKS], [

AC_MSG_CHECKING(
[kernel compilation mode])


CXXFLAGS="$CXXFLAGS -ffreestanding"
CXXFLAGS="$CXXFLAGS -fno-exceptions"
CXXFLAGS="$CXXFLAGS -fno-rtti"
CXXFLAGS="$CXXFLAGS -mno-red-zone"


AC_DEFINE(
[BLOCKOS_FREESTANDING],
1,
[Kernel freestanding mode])


AC_MSG_RESULT(
[enabled])

])
