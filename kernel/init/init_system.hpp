#pragma once
#include "unit_registry.hpp"

namespace init {

enum class UnitState : uint8_t {
    INACTIVE = 0,
    STARTING = 1,
    ACTIVE   = 2,
    FAILED   = 3,   // ExecStart betöltése/indítása sikertelen volt
    SKIPPED  = 4,   // egy Requires= függősége FAILED, ezért nem is próbáltuk elindítani
};

struct UnitRuntime {
    UnitState state = UnitState::INACTIVE;
    int32_t   pid = -1; // preempt_create_process_from_elf visszatérési értéke, -1 ha nem indult
};

class InitSystem {
public:
    UnitRegistry registry;
    UnitRuntime  runtime[MAX_UNITS];

    // Beolvassa a `dir_prefix` alatti .service fájlokat, topologikus
    // sorrendbe rendezi őket, majd sorban elindítja mindegyiket:
    //   - ExecStart bináris beolvasása vfs::read_file-lal
    //   - preempt_create_process_from_elf hívása a bináris tartalmára
    //   - Requires= függőség FAILED -> ez a unit SKIPPED, nem próbáljuk
    //   - ciklikus függőségű unitok SKIPPED-ként jelennek meg
    //
    // Return: true, ha minden unit ACTIVE lett (0 FAILED, 0 SKIPPED).
    bool boot(const char *dir_prefix);

    // Emberi olvasásra: kiírja (Print-tel) az összes unit végállapotát.
    void print_summary() const;
};

} // namespace init
