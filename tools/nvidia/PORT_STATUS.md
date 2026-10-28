# BlockOS NVIDIA port — implementációs állapot

Ez a `cpy/` csomag a BlockOS saját PCI/PCIe- és VFS-rétegéhez illeszkedő platformréteget adja. A teljes NVIDIA open-kernel-modules driver még nem futtatható vele önmagában: a jelenlegi BlockOS forrásban nincs olyan általános kernel API, amely tetszőleges fizikai MMIO-tartományt cache-policyval leképez, illetve PCI MSI/MSI-X megszakítást telepít. A valódi NVIDIA Resource Manager / GSP protokoll bekötése szintén külön feladat.

## Új fájlok

- `drivers/nvidia_probe.cpp/.hpp`: NVIDIA PCI display-eszköz felderítése a BlockOS meglévő PCI API-jával.
- `drivers/nvidia_port.cpp/.hpp`: határellenőrzött MMIO 32 bites olvasás/írás, DMA-eredmény ellenőrzése és nullázása, PCI Memory Space/Bus Master vezérlés, VFS-ből biztonságosan másolt firmware, IRQ- és időzítő-absztrakció, GSP bridge-hívások.
- `drivers/nvidia_dma_pool.cpp/.hpp`: rögzített bitmapes, összefüggő fizikai RAM-poolból DMA-területeket oszt ki. Nem téveszti össze a virtuális címet a DMA-címmel. Csak a BlockOS által előre lefoglalt és leképezett fizikai régiót használhatja.
- `drivers/nvidia_blockos_binding.cpp/.hpp`: a DMA-poolt és a BlockOS-specifikus MMIO/IRQ-kapcsolókat `PlatformOps`-szá köti össze; x86-64 TSC-alapú időt és IRQ-állapotmentést ad.
- `drivers/nvidia_device.cpp`: eszközéletciklus. A BAR0 méretét csak kikapcsolt PCI I/O-, memory- és bus-master bitekkel méri fel; először MMIO-t képez le, Bus Master csak DMA-területek után kapcsolható be, leválasztáskor a DMA-t előbb leállítja, és a DMA-puffereket csak utána szabadítja fel.
- `tools/nvidia/build_nvidia_os_agnostic_core.sh`: az NVIDIA upstream `src/nvidia` magobjektumát külön fordítja, Linux `.ko` készítése nélkül.

A BlockOS jelenlegi Makefile-ja automatikusan felveszi a `drivers/*.cpp` fájlokat, tehát a négy új driver `.cpp` fájlhoz nem kell kézzel forráslistát szerkeszteni.

## Mi készült el konkrétan

1. **Memória/DMA:** firmware-hez VFS-ből másolt, méretkorlátos CPU-puffer; fizikailag összefüggő DMA-pool allocator; cím-, méret-, igazítás- és maximum DMA-cím ellenőrzés; minden sikeres DMA-foglalás nullázása; koherens pool esetén memória-fence. Nem koherens DMA-pool `sync` művelete szándékosan hibát jelez, amíg valódi cache-karbantartás nincs bekötve.
2. **MMIO:** fizikai és virtuális címet külön kezel; a map/unmapot a BlockOS page-table kódjára bízza; a 32 bites regiszterműveletek ellenőrzik az igazítást és a leképezés határait.
3. **Megszakítás:** IRQ-kezelő regisztrálás/eltávolítás és IRQ-állapotmentési hívási felület; a tényleges IDT/APIC útvonalat a BlockOS kernel IRQ-szolgáltatásához kell kötni.
4. **Firmware/GSP:** VFS-ből másolt firmware, 32 MiB-es felső korlát, DMA-ba másolás és szinkronizálás, bootstrap scratch-buffer, GSP boot/service állapotgép-hívás.
5. **Eszközkezelés:** biztonságosabb BAR-mérés, PCI Memory Space és Bus Master külön vezérlése, erőforrások nyilvántartása és visszafelé sorrendű felszabadítás.

## A valódi hardveres induláshoz még bekötendő pontok

A `BlockOSKernelServices` MMIO callbackjeinek valódi page-table map/unmap műveletet kell végrehajtaniuk Uncached attribútummal. Az IRQ callbackeknek a kernel IDT/APIC szolgáltatásán keresztül kell útvonalat foglalniuk és megszüntetniük. A DMA-pool alapjául szolgáló RAM-tartományt a boot/memóriakezelőnek kell lefoglalnia úgy, hogy azt más kernel-allokátor ne használja.

A `gsp_boot` és `gsp_service` szándékosan nem kitalált regiszter-offsetekkel vagy saját, ellenőrizetlen parancsformátummal működik: ezeket az NVIDIA upstream Resource Manager/GSP kód tényleges, BlockOS-ra portolt része kell hogy adja. Ha nincs megadva, `device_boot_gsp()` `GspUnavailable` állapottal leáll, nem kapcsolja be a Bus Mastert, és nem állítja azt, hogy a kártya inicializálva van.

## Bekötés vázlat helyett — szükséges kernel-adatok

A kernel indulásakor:

1. Foglalj le egy **fizikailag összefüggő**, page-aligned DMA RAM-régiót, és add meg a kernel-virtuális leképezését. A bitmap mérete legalább `(pool_size / 4096 + 7) / 8` byte legyen. A tartományt vond ki minden más allokátorból.
2. `dma_pool_init(&pool, virtual_base, physical_base, size, bitmap, bitmap_bytes, true)` csak akkor kapjon `true` értéket, ha a DMA-memória valóban koherens az adott platformon; ellenkező esetben valódi cache-sync megvalósítás kell.
3. Töltsd ki a `BlockOSKernelServices` `map_mmio`, `unmap_mmio`, `irq_register`, `irq_unregister` műveleteit a kernel tényleges MMIO- és IRQ-szolgáltatásával. Ne használj egyszerű `physical_address` → pointer castot, hacsak a kernel garantáltan megfelelően leképezte a lapot és a cache-típust is beállította.
4. A `gsp_boot` és `gsp_service` callbackeket csak azután állítsd be, hogy az upstream NVIDIA RM/GSP init- és parancsútvonal ténylegesen át lett portolva.
5. Készíts `BlockOSBinding` példányt, hívd a `make_blockos_platform_ops(&binding, &ops)` függvényt, majd `install_platform_ops(&ops)` és `attach_first(&device)`.

A `BlockOSBinding` objektumnak, a DMA-poolnak és a kernel callbackek által hivatkozott adatoknak a driver teljes élettartama alatt élniük kell.

## Tesztek

A `tests/run_nvidia_port_tests.sh` hoston fake PCI/MMIO/IRQ/GSP backenden futtatja az új réteg tesztjeit. Ez ellenőrzi a DMA-poolt, MMIO-határvizsgálatot, PCI Bus Master sorrendet, firmware-staginget, GSP állapotát és detach alatti felszabadítást. Ez **host-mock teszt**, nem fizikai NVIDIA GPU-teszt, és nem bizonyítja az upstream `nv-kernel.o` lefordítását vagy a GSP valódi elindulását.
