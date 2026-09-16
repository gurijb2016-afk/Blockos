# BlockOS — integrált csomag: valódi többfolyamatos userspace

Ez az **egy csomag**, ami az eddigi összes lépést tartalmazza összeillesztve,
plusz a hiányzó összekötő darabokat (boot-integráció, build script, két
valódi teszt-processz).

---

## ELŐSZÖR EZT OLVASD EL: mi fut ettől, és mi nem

**Ami ettől ténylegesen működni fog:**
- Két (vagy több) **valódi ring3 processz fut EGYSZERRE**, preemptíven váltva
- Működő libc: `malloc`, `printf`, `string.h`, fájl-I/O
- Dinamikus linkelés infrastruktúrája (ld.so) készen áll

**Ami ettől MÉG NEM fog futni: az fvwm3.** Ez a csomag a 7-lépéses listából
az 1-4. pontot fedi le, plusz a valódi preemptív multitaskingot. Az fvwm3-hoz
még hiányzik:
- **X11 IPC-csatorna** (a kernelben nincs socket; `SYS_socket` `ENOSYS`)
- **Per-processz fd-tábla** — jelenleg a fájlleírók GLOBÁLISAK, két processz
  osztozik rajtuk; ez egy X szerver+kliens párosnál biztosan hibát okoz
- **Xlib port**
- **X szerver** (valódi Xorg, vagy saját minimál wire-protocol szerver)

Ez nem kifogás, hanem a pontos állapot: ezek nélkül nincs mit elindítani.
A most átadott rész viszont **önmagában tesztelhető és bizonyítható** — és
minden további erre épül.

---

## Fájlok: mit hova

| Csomagbeli fájl | Hova a repódban | Művelet |
|---|---|---|
| `kernel/preempt.hpp` | `kernel/preempt.hpp` | **ÚJ** |
| `kernel/preempt.cpp` | `kernel/preempt.cpp` | **ÚJ** |
| `kernel/elf_loader.hpp` | `kernel/elf_loader.hpp` | csere |
| `kernel/elf_loader.cpp` | `kernel/elf_loader.cpp` | csere |
| `kernel/process.hpp` | `kernel/process.hpp` | csere |
| `kernel/process.cpp` | `kernel/process.cpp` | csere |
| `kernel/user_syscall.cpp` | `kernel/user_syscall.cpp` | csere |
| `arch/86_64x/hardware_tables.cpp` | ugyanoda | csere |
| `kernel/boot_launch.inc` | — | **kézzel beilleszteni**, lásd alább |
| `userspace/libc/**` | ugyanoda | ÚJ mappa |
| `userspace/ldso/**` | ugyanoda | ÚJ mappa |
| `userspace/tests/proc_a.c`, `proc_b.c` | ugyanoda | ÚJ |
| `scripts/build-all.sh` | ugyanoda | ÚJ |

**Az egyetlen kézi lépés:** a `kernel/kernel.cpp` végén lévő „First userspace
program" blokkot (ami `/bin/sh`-t olvas, `process::create`, `process::run`)
cseréld le a `kernel/boot_launch.inc` tartalmára. A fájl eleje pontosan
leírja, melyik blokkot kell megtalálni.

Ne felejtsd a build-rendszeredhez hozzáadni a `kernel/preempt.cpp`-t.

---

## Build és futtatás

```sh
# a repo gyökeréből:
sh scripts/build-all.sh
```

Majd másold a rootfs image-be:
```
build/proc_a  ->  /bin/proc_a
build/proc_b  ->  /bin/proc_b
build/ld.so   ->  /system/lib/ld.so     (csak dinamikus binárishoz kell)
```

Fordítsd újra a kernelt, és bootolj.

### Mit KELL látnod

```
boot: spawned /bin/proc_a as pid 1
boot: spawned /bin/proc_b as pid 2
boot: starting scheduler with 2 process(es)
[A] started, argc=1, pid=1
[B] started, argc=1, pid=2
[A] tick 0
[B] tick 0
[A] tick 1
[B] tick 1
...
```

**Az ÖSSZEFÉSÜLT kimenet a bizonyíték**, hogy a preemptív váltás működik.

Ha ehelyett előbb az összes `[A]`, aztán az összes `[B]` jön, akkor a
timer-preempció nem üt be — ellenőrizd, hogy a `hardware_tables.cpp` cserélve
lett-e, és hogy a `pit_init(100)` + `sti` továbbra is lefut az
`init()`-ben.

---

## Amit ELLENŐRIZTEM (és amit nem)

Ellenőrizve:
1. **A valódi kernel ELF-betöltő betöltötte a valódi lefordított binárist.**
   Nem szimuláció: lefordítottam a `proc_a`-t a libc-vel, majd átadtam az
   igazi `elf_loader.cpp`-nek egy mock lapkezelővel. Eredmény: entry
   `0x401000`, 5 lap mappolva, és a belépési ponton a bájtok `48 31 ed 48`
   — ami pontosan a `crt1.S` `xor %rbp,%rbp` utasítása. A betöltő tehát
   valódi kódot másol, nem nullákat.
2. **Az auxv-stack helyes**: `argc=1`, és az `AT_ENTRY` bájtra egyezik a
   betöltött entry ponttal.
3. **Scheduler: 18 funkcionális teszt** a valódi `preempt.cpp`-vel —
   ring3 preempció, teljes regiszter-mentés/visszaállítás, round-robin,
   exit-átadás, és hogy ring0 kernelkód SOHA nem preemptálódik.
4. **Fordítási idejű layout-ellenőrzés** `static_assert`-ekkel: a
   keret-struktúrák bájtra egyeznek az `irq_stubs.S` push-sorrendjével.
5. **Teljes userspace build**: libc + `crt1.o` + mindkét teszt-program
   ténylegesen lefordult és linkelt, undefined symbol nélkül. Az `ld.so`
   szintén lefordult, helyes ET_DYN típussal.

NEM ellenőrizve (nálad kell):
- Tényleges futás QEMU-n/hardveren, élő laptáblákkal és valódi CR3-váltással
- Időzítés-érzékeny viselkedés valós PIT-megszakításokkal
- A dinamikus linkelés teljes útvonala (a teszt-programok szándékosan
  statikusak, hogy a scheduler külön legyen tesztelhető az ld.so-tól —
  egyszerre egy dolgot érdemes hibakeresni)

---

## Ismert korlátok, amik a következő lépéseket érintik

1. **Az fd-tábla globális, nem per-processz** (`user_syscall.cpp`). Két
   párhuzamos processz osztozik a fájlleírókon. Az X szerver+kliens
   párosnál ezt MUSZÁJ lesz javítani.
2. **A kernel nem preemptálható** (egyetlen közös `rsp0`). Blokkoló
   syscall (pl. „várj üzenetre") írása előtt per-task kernel stack kell.
3. **Nincs erőforrás-felszabadítás** kilépő processz után (pml4, lapok).
4. **Nincs `SYS_clone`** — a `pthread_create` ezért hibát ad vissza.
5. A teszt-programok `-no-pie -static`-ok; a betöltő boot-útvonala ET_EXEC-et
   vár.
