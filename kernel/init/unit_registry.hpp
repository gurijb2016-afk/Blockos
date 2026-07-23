#pragma once
#include "unit_file.hpp"

namespace init {

constexpr size_t MAX_UNITS = 64;

class UnitRegistry {
public:
    UnitFile units[MAX_UNITS];
    uint32_t count = 0;

    // Végigmegy a vfs teljes (lapos névterű) fájllistáján, és minden
    // olyan bejegyzést feldolgoz, ami `dir_prefix`-szel kezdődik és
    // ".service"-re végződik (pl. dir_prefix = "/system/services/").
    // Érvénytelen (ExecStart nélküli) unit fájlokat kihagyja, és a
    // hívó felé jelzi a nevüket a `bad_units_out`/`bad_units_count`
    // paramétereken keresztül (opcionális, lehet nullptr).
    //
    // Return: sikeresen betöltött unitok száma.
    uint32_t scan(const char *dir_prefix,
                  const char **bad_units_out = nullptr,
                  uint32_t *bad_units_count = nullptr,
                  uint32_t bad_units_capacity = 0);

    // Megkeres egy unitot név szerint (pl. "network.service").
    // Return: mutató a belső táblába, vagy nullptr ha nincs ilyen.
    UnitFile *find(const char *name);
};

} // namespace init
