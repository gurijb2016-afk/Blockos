#!/bin/sh
# build-all.sh — builds the complete userspace side in one go.
# Run from the ROOT of your BlockOS repo (the dir containing kernel/ and userspace/).
set -eu

CC=${CC:-x86_64-elf-gcc}
AS=${AS:-x86_64-elf-as}
AR=${AR:-x86_64-elf-ar}

OUT=build
mkdir -p "$OUT"

echo "==> 1/4  libc"
LIBC_OUT="$OUT/libc"
mkdir -p "$LIBC_OUT"
CFLAGS="-std=c11 -O2 -ffreestanding -fno-builtin -fno-stack-protector -fpic -Iuserspace/libc/include"
"$AS" --64 -o "$LIBC_OUT/crt1.o" userspace/libc/src/crt1.S
for f in userspace/libc/src/*.c; do
    "$CC" $CFLAGS -c "$f" -o "$LIBC_OUT/$(basename "$f" .c).o"
done
"$AR" rcs "$LIBC_OUT/libc.a" "$LIBC_OUT"/*.o
"$AR" d "$LIBC_OUT/libc.a" crt1.o 2>/dev/null || true
echo "    -> $LIBC_OUT/libc.a  $LIBC_OUT/crt1.o"

echo "==> 2/4  ld.so"
"$CC" -std=c11 -O1 -fpie -fno-stack-protector -fno-builtin -ffreestanding \
      -nostdlib -static-pie -Wl,-e,_start -Iuserspace/ldso \
      userspace/ldso/start.S userspace/ldso/ldso.c -o "$OUT/ld.so"
echo "    -> $OUT/ld.so"

echo "==> 3/4  test programs (statically linked)"
# Built -no-pie / -static on purpose:
#   * the kernel's ELF loader path used at boot expects ET_EXEC
#   * static means they do NOT need ld.so, so you can verify the scheduler
#     independently of the dynamic-linking path. Test one thing at a time.
for prog in proc_a proc_b; do
    "$CC" -std=c11 -O1 -ffreestanding -fno-builtin -fno-stack-protector \
          -no-pie -static -nostdlib \
          -Iuserspace/libc/include \
          "$LIBC_OUT/crt1.o" "userspace/tests/$prog.c" "$LIBC_OUT/libc.a" \
          -Wl,-e,_start -o "$OUT/$prog"
    echo "    -> $OUT/$prog"
done

echo "==> 4/4  done"
cat <<'EOF'

Copy into your rootfs image:

    build/proc_a   ->  /bin/proc_a
    build/proc_b   ->  /bin/proc_b
    build/ld.so    ->  /system/lib/ld.so     (only needed once you build
                                              something DYNAMICALLY linked)

Then rebuild the kernel with the boot_launch.inc change applied and boot it.

Expected result: [A] and [B] lines INTERLEAVED in the output.
If they come out sequentially (all A, then all B), the timer preemption
is not firing - check that hardware_tables.cpp was replaced and that
pit_init(100)/sti still run in HardwareTablesManager::init().
EOF
