#include "dependency_graph.hpp"
#include <string.h>

namespace {

// Megkeresi egy unit nevének registry-indexét, vagy -1-et ad ha nincs
// ilyen néven regisztrált unit (phantom függőség — figyelmen kívül
// hagyjuk, nem hiba).
int32_t find_index(const init::UnitRegistry &reg, const char *name) {
    for (uint32_t i = 0; i < reg.count; i++) {
        if (strcmp(reg.units[i].name, name) == 0) return (int32_t)i;
    }
    return -1;
}

} // namespace

void init::topo_sort(const UnitRegistry &reg, TopoResult *out) {
    memset(out, 0, sizeof(*out));

    uint32_t n = reg.count;
    if (n == 0) return;

    // adjacency[i][j] = true, ha j-nek előbb kell futnia mint i-nek
    // (j -> i él: "i After/Requires j"). Fix méretű mátrix, mert
    // MAX_UNITS kicsi (64) és nincs dinamikus allokáció.
    static bool adjacency[MAX_UNITS][MAX_UNITS];
    memset(adjacency, 0, sizeof(adjacency));
    uint32_t indegree[MAX_UNITS] = {0};

    for (uint32_t i = 0; i < n; i++) {
        const UnitFile &u = reg.units[i];

        for (uint8_t k = 0; k < u.after_count; k++) {
            int32_t j = find_index(reg, u.after[k]);
            if (j < 0 || (uint32_t)j == i) continue; // phantom vagy önhivatkozás
            if (!adjacency[j][i]) {
                adjacency[j][i] = true;
                indegree[i]++;
            }
        }
        for (uint8_t k = 0; k < u.requires_count; k++) {
            int32_t j = find_index(reg, u.requires_[k]);
            if (j < 0 || (uint32_t)j == i) continue;
            if (!adjacency[j][i]) {
                adjacency[j][i] = true;
                indegree[i]++;
            }
        }
    }

    // Kahn: kezdjük azokkal, akiknek nincs függőségük.
    uint32_t queue[MAX_UNITS];
    uint32_t qhead = 0, qtail = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (indegree[i] == 0) queue[qtail++] = i;
    }

    uint32_t processed = 0;
    while (qhead < qtail) {
        uint32_t cur = queue[qhead++];
        out->order[out->order_count++] = cur;
        processed++;

        for (uint32_t j = 0; j < n; j++) {
            if (adjacency[cur][j]) {
                indegree[j]--;
                if (indegree[j] == 0) queue[qtail++] = j;
            }
        }
    }

    if (processed < n) {
        // Van ciklus: minden unit, ami nem került be az order[]-be,
        // részt vesz egy ciklusban (vagy egy ciklustól függ).
        out->has_cycle = true;
        for (uint32_t i = 0; i < n; i++) {
            bool in_order = false;
            for (uint32_t k = 0; k < out->order_count; k++) {
                if (out->order[k] == i) { in_order = true; break; }
            }
            if (!in_order) out->cyclic[out->cyclic_count++] = i;
        }
    }
}
