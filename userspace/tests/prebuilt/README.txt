BlockOS dynamic ELF sanity set

These x86-64 ELF files were built with the host GNU toolchain but deliberately
use the BlockOS syscall ABI from userspace/libc and the interpreter path
/system/lib/ld.so. They are included as a structural/runtime smoke test for
PT_INTERP + libc.so + libpthread.so + TLS.

Install:
  /system/lib/ld.so
  /system/lib/libc.so
  /system/lib/libpthread.so
  /system/bin/dynamic-tls

The test prints its main-thread TLS value and creates a pthread with its own
TLS/errno state. A successful run proves the basic dynamic-loader/libc/TLS
chain, but it is not a substitute for the full GNOME build.

The files are host-built because the current development environment does not
contain x86_64-elf-gcc/as/ar. Rebuild them with userspace/libc/build-libc.sh
and userspace/tests/build-dynamic-tls.sh when the BlockOS cross toolchain is
available.
