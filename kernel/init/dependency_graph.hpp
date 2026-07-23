#pragma once
#include "unit_registry.hpp"

namespace init {

struct TopoResult {
    uint32_t order[MAX_UNITS];   // registry-indexek indítási sorrendben
    uint32_t order_count;
    bool     has_cycle;
    // Ha has_cycle igaz: a ciklusban részt vevő (ezért sosem
    // ütemezett) unitok indexei itt vannak, order_count..MAX_UNITS
    // tartományban nem garantált sorrend.
    uint32_t cyclic[MAX_UNITS];
    uint32_t cyclic_count;
};

// Kahn-algoritmus a UnitFile.after / UnitFile.requires_ mezőkből
// épített függőségi gráfon. A "X After=Y" azt jelenti: Y-nak a
// registry-ben előbb kell szerepelnie az order[]-ben, mint X-nek.
// A "Requires=" ugyanúgy sorrendi élt ad (a tényleges "ha Y elbukik,
// X se induljon" logikát az init_system.cpp kezeli futásidőben,
// mert az a unitok állapotától függ, nem csak a statikus gráftól).
//
// Az After=/Requires= bejegyzések, amik nem hivatkoznak egy a
// registry-ben ténylegesen létező unitra, figyelmen kívül vannak
// hagyva (nincs "phantom" függőség).
void topo_sort(const UnitRegistry &reg, TopoResult *out);

} // namespace init
