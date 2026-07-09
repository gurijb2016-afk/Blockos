AC_INIT([BlockOS],[3.0])

AC_CONFIG_MACRO_DIR([m4])

AC_CANONICAL_HOST


dnl ==================================================
dnl Compiler and Toolchain
dnl ==================================================

BLOCKOS_CHECK_COMPILER
BLOCKOS_CHECK_LINKER
BLOCKOS_CHECK_ASSEMBLER
BLOCKOS_CHECK_BINUTILS


dnl ==================================================
dnl Build Environment
dnl ==================================================

BLOCKOS_ENVIRONMENT
BLOCKOS_BUILD_INFO
BLOCKOS_BUILD_TIMESTAMP


dnl ==================================================
dnl Architecture
dnl ==================================================

BLOCKOS_ARCH_CHECK
BLOCKOS_CPU_FEATURES
BLOCKOS_64BIT_CHECK


dnl ==================================================
dnl Kernel Core
dnl ==================================================

BLOCKOS_KERNEL_CHECKS
BLOCKOS_MEMORY_SYSTEM
BLOCKOS_KERNEL_FLAGS
BLOCKOS_STACK_PROTECTION


dnl ==================================================
dnl Boot System
dnl ==================================================

BLOCKOS_UEFI_CHECK
BLOCKOS_GOP_CHECK
BLOCKOS_BOOT_PROTOCOL


dnl ==================================================
dnl Memory Subsystem
dnl ==================================================

BLOCKOS_PAGING_CHECK
BLOCKOS_HEAP_CHECK
BLOCKOS_ALLOCATOR_CHECK


dnl ==================================================
dnl Process System
dnl ==================================================

BLOCKOS_SCHEDULER_CHECK
BLOCKOS_PROCESS_MANAGER_CHECK
BLOCKOS_CONTEXT_SWITCH_CHECK


dnl ==================================================
dnl Executable System
dnl ==================================================

BLOCKOS_ELF_CHECK
BLOCKOS_ABI_CHECK
BLOCKOS_SYSCALL_CHECK


dnl ==================================================
dnl Filesystem
dnl ==================================================

BLOCKOS_VFS_CHECK
BLOCKOS_JOURNAL_CHECK
BLOCKOS_STORAGE_CHECK


dnl ==================================================
dnl Drivers
dnl ==================================================

BLOCKOS_PCI_CHECK
BLOCKOS_VIRTIO_CHECK
BLOCKOS_INPUT_CHECK
BLOCKOS_FRAMEBUFFER_CHECK


dnl ==================================================
dnl Graphics
dnl ==================================================

BLOCKOS_GUI_CHECK
BLOCKOS_WINDOW_CHECK
BLOCKOS_FONT_CHECK


dnl ==================================================
dnl User Space
dnl ==================================================

BLOCKOS_SHELL_CHECK
BLOCKOS_USER_PROGRAM_CHECK


dnl ==================================================
dnl Security
dnl ==================================================

BLOCKOS_SECURITY_CHECK
BLOCKOS_PERMISSION_CHECK


dnl ==================================================
dnl Output Files
dnl ==================================================

AC_CONFIG_FILES([
Makefile
])

AC_OUTPUT
