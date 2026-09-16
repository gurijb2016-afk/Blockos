# ld.so — MVP implementation

Fájlok:
- `elf_abi.h` — a szükséges ELF64 dinamikus struktúrák/konstansok
- `start.S` — belépési pont (`_start`), és a `ldso_jump_to_entry` trambulin
- `ldso.c` — a tényleges linker-logika
- `build-ldso.sh` — fordítás a te toolchainedhez (`x86_64-elf-gcc`)

## Ellenőrzés, amit itt el tudtam végezni

- `ldso.c`: `gcc -fsyntax-only -ffreestanding -m64` — tiszta (1 db ártalmatlan
  unused-parameter warning).
- `start.S`: valódi `as --64`-fel lefordítva, `objdump`-pal ellenőrizve — a
  gépi kód pontosan azt csinálja, amit kell (rsp → rdi, call ld_main, exit).
- A TELJES láncot (a te UEFI kernelend + a te toolchained + tényleges
  betöltés) NEM tudtam itt lefuttatni — ahhoz a te gépeden/build
  környezetedben kell tesztelni.

## Frissítés: mostantól valódi fájl-hátterű mmap-ot használ

A `read_whole_file` már NEM `mmap(anonymous)+read()`-ciklust csinál, hanem
egyetlen fájl-hátterű `mmap` hívással kapja meg a teljes fájltartalmat — ehhez
tartozik a `kernel/user_syscall.cpp` frissítése is (lásd külön csomagban),
ami a `flags`/`fd`/`offset` paramétereket a valódi Linux-szerű `r10`/`r8`/`r9`
regiszterekből olvassa. Az `sc_mmap()` ezeket a regisztereket explicit módon
állítja be (`register long r10 __asm__("r10")=...`), így nem hagyatkozik
"amilyen regiszter épp szabad" viselkedésre, mint a régi általános `sc()`
helper tenné 6+ argumentumnál.

Ezt valódi `gcc -O1 -fpic -c`-vel is lefordítottam (nem csak
`-fsyntax-only`-val) és `objdump`-pal visszaellenőriztem, hogy a
szintaxis és a regiszter-megkötések helyesen generálódnak — a tényleges
kernel-integrációt nálad kell tesztelni.

## Hogyan illeszkedik a korábbi patch-hez

A kernel-oldali patch (elf_loader/process) `AT_BASE`-be teszi az ld.so saját
load biasát, `AT_PHDR`/`AT_PHENT`/`AT_PHNUM`-ba a FŐ PROGRAM (nem a ld.so!)
programfejléceit, `AT_ENTRY`-be pedig a fő program valódi belépési pontját.
Az `ld_main()` pontosan ezt olvassa ki a nyers stackről.

## Amit ez az MVP TUD

- Beolvassa a saját auxv/argv/envp adatait a nyers kernel-stackről
- Megkeresi a fő program `PT_DYNAMIC` szegmensét `AT_PHDR` alapján
- Rekurzívan betölti a `DT_NEEDED` shared library-kat `/system/lib/<name>`
  alól (`openat`+`fstat`+`read` a te syscalljaiddal)
- Minden betöltött objektumot egy saját, fix címre mappol (`mmap` a te
  3-paraméteres `do_mmap`-oddal, RWX móddal — lásd limitációk)
- Relokációkat alkalmaz: `R_X86_64_RELATIVE`, `R_X86_64_GLOB_DAT`,
  `R_X86_64_JUMP_SLOT`, `R_X86_64_64`
- A végén visszaállítja az EREDETI stack pointert és ugrik a fő program
  valódi entry pontjára

## Konkrét, dokumentált egyszerűsítések (ezeket kell tudni, mielőtt élesben
## bíznál benne)

1. **Csak ET_EXEC (nem-PIE) fő programot kezel.** Ha az fvwm3-at vagy
   bármelyik libet PIE-ként (`-pie`) fordítod, a `main_dyn` cím-számítás
   hibás lesz — fordítsd a fő userspace bináriakat `-no-pie`-vel.
2. **Nincs lazy binding (PLT stub-ok).** Minden `R_X86_64_JUMP_SLOT`-ot
   azonnal, indításkor felold — egyszerűbb és robosztusabb egy MVP-nek,
   de kicsit lassabb indítás nagy programoknál.
3. **A dinamikus szimbólumtábla méretét (`nsyms`) heurisztikával becsli**
   (feltételezi, hogy a linker közvetlenül a `.dynsym` után teszi a
   `.dynstr`-t — ez a GNU ld alapértelmezett elrendezése, de ha valamelyik
   toolchain mást csinál, ez 0-t ad, és a szimbolikus relokációk nem
   oldódnak fel). Ha ez gondot okoz, a valódi javítás egy `DT_HASH`
   feldolgozó hozzáadása (`nchain` mező = szimbólumszám).
4. **`R_X86_64_COPY` nincs kezelve** — exportált adat-szimbólumok
   (pl. bizonyos libc globális változók) másolásos relokációja hiányzik.
5. **Minden lapot RWX-nek mappol** (nincs külön írható/futtatható
   szétválasztás) — működik, de nem biztonságos, csak bootstrap-célra jó.
6. **Nincs önmagát relokáló lépés** — szándékosan úgy írtam meg a kódot
   (nincs függvénypointer-inicializálású írható globális), hogy `-fpie`
   alatt ehhez ne legyen szükség; ha bővíted a fájlt ilyen mintával, ezt a
   lépést pótolni kell.
7. **Egyetlen, kőbe vésett keresési útvonal**: `/system/lib/<name>` — nincs
   `LD_LIBRARY_PATH`, nincs `RPATH`/`RUNPATH` feldolgozás.

## Tesztelési javaslat NÁLAD

Mielőtt fvwm3-at vagy Xlib-et próbálnál rajta futtatni, egy sokkal kisebb
tesztre van szükség: fordíts egy triviális `.so`-t (egy exportált
függvénnyel) és egy hozzá dinamikusan linkelt kis programot a te
toolchaineddel (`-no-pie -shared` / `-no-pie` és linkeld a `.so`-hoz), tedd
a `.so`-t `/system/lib/`-be a rootfs-ben, ezt az ld.so-t pedig `/system/lib/ld.so`
néven, és nézd meg, célba ér-e a hívás. Ez jóval kisebb és gyorsabban
hibakereshető lépés, mint egyből az Xlib-fvwm3 láncot próbálni.
