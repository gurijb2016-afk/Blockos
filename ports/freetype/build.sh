#!/bin/sh
FT=$1
F="-m64 -fPIC -fno-stack-protector -mno-red-zone -fno-exceptions -D__blockos__ -DFT2_BUILD_LIBRARY -include stdlib.h -include string.h -include $PWD/ft/shim/stdlib_extra.h -O2 -w -nostdinc -isystem $PWD/bos/userspace/libc/include -isystem ./bos31/userspace/libc/include -isystem /usr/lib/llvm-18/lib/clang/18/include -I$PWD/ft/shim -I$FT/include -I$FT/builds -I$FT/src"
OK=0; FAIL=0
for f in $FT/src/base/ftbase.c $FT/src/base/ftinit.c $FT/src/base/ftsystem.c $FT/src/base/ftdebug.c $FT/src/base/ftglyph.c $FT/src/base/ftbitmap.c $FT/src/base/ftmm.c $FT/src/base/ftfntfmt.c $FT/src/base/ftgasp.c $FT/src/base/ftsynth.c $FT/src/base/ftpsprop.c $FT/src/base/ftstroke.c $FT/src/base/ftutil.c $FT/src/base/ftfstype.c $FT/src/base/ftcalc.c $FT/src/base/ftobjs.c $FT/src/base/ftoutln.c $FT/src/base/ftrfork.c $FT/src/base/ftlcdfil.c $FT/src/base/ftsnames.c $FT/src/base/ftpatent.c $FT/src/smooth/smooth.c $FT/src/sfnt/sfnt.c $FT/src/truetype/truetype.c $FT/src/cff/cff.c $FT/src/psnames/psnames.c $FT/src/psaux/psaux.c $FT/src/pshinter/pshinter.c $FT/src/raster/raster.c $FT/src/type1/type1.c $FT/src/bdf/bdf.c $FT/src/pcf/pcf.c $FT/src/pfr/pfr.c $FT/src/winfonts/winfnt.c $FT/src/type42/type42.c $FT/src/svg/svg.c $FT/src/sdf/sdf.c $FT/src/psnames/psmodule.c; do
  [ -f "$f" ] || { FAIL=$((FAIL+1)); echo "missing $f"; continue; }
  o=ft/obj/$(echo "$f" | tr '/' '_').o
  if clang $F -c "$f" -o "$o" 2>ft/obj/err.txt; then OK=$((OK+1)); else FAIL=$((FAIL+1)); echo "FAIL $f: $(grep -m1 error: ft/obj/err.txt)"; fi
done
echo "ok=$OK fail=$FAIL"
